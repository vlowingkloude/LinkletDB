#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transaction.h"
#include "binder.h"
#include "catalog.h"
#include "executor.h"
#include "gql_parser.h"
#include "wal.h"

enum {
    LINKLET_TRANSACTION_INITIAL_CALL_CAPACITY = 4,
    LINKLET_TRANSACTION_INITIAL_OPERATION_CAPACITY = 8,
    LINKLET_TRANSACTION_CAPACITY_GROWTH_FACTOR = 2,
};

static void set_error(LinkletError *error, const char *message) {
    if (error) {
        snprintf(error->message, sizeof(error->message), "%s", message ? message : "");
    }
}

static bool append_call(LinkletTransaction *transaction, LinkletKernelCall *call) {
    if (transaction->call_count == transaction->capacity) {
        const size_t capacity =
            transaction->capacity
                ? transaction->capacity * LINKLET_TRANSACTION_CAPACITY_GROWTH_FACTOR
                : LINKLET_TRANSACTION_INITIAL_CALL_CAPACITY;
        LinkletKernelCall *grown =
            (LinkletKernelCall *)realloc(transaction->calls, capacity * sizeof(*grown));
        if (!grown) {
            return false;
        }
        transaction->calls = grown;
        transaction->capacity = capacity;
    }
    transaction->calls[transaction->call_count++] = *call;
    *call = (LinkletKernelCall){0};
    return true;
}

static void clear_transaction(LinkletTransaction *transaction) {
    if (!transaction) {
        return;
    }
    for (size_t index = 0; index < transaction->call_count; ++index) {
        linklet_kernel_call_destroy(&transaction->calls[index]);
    }
    free(transaction->calls);
    *transaction = (LinkletTransaction){0};
}

bool linklet_database_begin(LinkletDatabase *database, LinkletTransaction *transaction,
                            LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!database || !transaction) {
        set_error(error, "database and transaction are required");
        return false;
    }
    *transaction = (LinkletTransaction){0};
    transaction->database = database;
    transaction->active = true;
    return true;
}

bool linklet_transaction_execute(LinkletTransaction *transaction, const char *gql,
                                 LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!transaction || !transaction->active || !gql) {
        set_error(error, "an active transaction and GQL text are required");
        return false;
    }

    GqlParseError parse_error;
    GqlNode *ast = gql_parse_program(gql, &parse_error);
    if (!ast) {
        if (error) {
            snprintf(error->message, sizeof(error->message), "parse error at %zu:%zu: %.180s",
                     parse_error.line, parse_error.column, parse_error.message);
        }
        return false;
    }
    if (linklet_is_catalog_ddl(ast)) {
        set_error(error, "catalog DDL is not supported inside a transaction");
        gql_node_free(ast);
        return false;
    }

    LinkletLogicalPlan plan = {.calls = NULL, .call_count = 0};
    bool success = linklet_bind(ast, &plan, error);
    gql_node_free(ast);
    if (!success) {
        return false;
    }

    if (plan.call_count != 1) {
        set_error(error, "multi-call logical plans are not supported in transactions yet");
        linklet_logical_plan_destroy(&plan);
        return false;
    }
    LinkletKernelCall *call = &plan.calls[0];
    if (call->code != LINKLET_KERNEL_INSERT && call->code != LINKLET_KERNEL_UPDATE &&
        call->code != LINKLET_KERNEL_DELETE) {
        set_error(error, "reads are not supported inside a transaction yet");
        linklet_logical_plan_destroy(&plan);
        return false;
    }
    if (!append_call(transaction, call)) {
        set_error(error, "out of memory buffering transaction");
        linklet_logical_plan_destroy(&plan);
        return false;
    }
    linklet_logical_plan_destroy(&plan);
    return true;
}

static bool append_operation(LinkletResolvedOperation **operations, size_t *count, size_t *capacity,
                             const LinkletResolvedOperation operation) {
    if (*count == *capacity) {
        const size_t next = *capacity ? *capacity * LINKLET_TRANSACTION_CAPACITY_GROWTH_FACTOR
                                      : LINKLET_TRANSACTION_INITIAL_OPERATION_CAPACITY;
        LinkletResolvedOperation *grown =
            (LinkletResolvedOperation *)realloc(*operations, next * sizeof(*grown));
        if (!grown) {
            return false;
        }
        *operations = grown;
        *capacity = next;
    }
    (*operations)[(*count)++] = operation;
    return true;
}

static char *wal_path(const LinkletDatabase *database) {
    const size_t size = strlen(database->path) + 1 + strlen("wal.ll") + 1;
    char *buffer = (char *)malloc(size);
    if (buffer) {
        snprintf(buffer, size, "%s/wal.ll", database->path);
    }
    return buffer;
}

bool linklet_transaction_commit(LinkletTransaction *transaction, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!transaction || !transaction->active) {
        set_error(error, "an active transaction is required");
        return false;
    }

    LinkletResolvedOperation *operations = NULL;
    size_t operation_count = 0, capacity = 0;
    uint64_t node_base = linklet_graph_store_node_id_capacity(transaction->database->store);
    uint64_t edge_base = linklet_graph_store_edge_id_capacity(transaction->database->store);
    bool success = true;

    for (size_t index = 0; success && index < transaction->call_count; ++index) {
        LinkletKernelCall *call = &transaction->calls[index];
        if (call->code == LINKLET_KERNEL_INSERT) {
            const bool is_node = call->insert_kind == LINKLET_ELEMENT_NODE;
            const LinkletResolvedOperation operation = {
                .operation = is_node ? LINKLET_WAL_INSERT_NODE : LINKLET_WAL_INSERT_EDGE,
                .id = is_node ? node_base++ : edge_base++,
                .source_id = call->insert_source_id,
                .destination_id = call->insert_destination_id,
                .detach = false,
                .payload = &call->payload,
            };
            success = append_operation(&operations, &operation_count, &capacity, operation);
        } else {
            LinkletIdList ids = {0};
            if (!linklet_resolve_match(transaction->database->store, &call->match, call->direction,
                                       &ids, error)) {
                success = false;
            } else {
                for (size_t match_index = 0; match_index < ids.count; ++match_index) {
                    const bool is_node = call->match.result_kind == LINKLET_ELEMENT_NODE;
                    LinkletResolvedOperation operation = {0};
                    if (call->code == LINKLET_KERNEL_UPDATE) {
                        operation.operation =
                            is_node ? LINKLET_WAL_UPDATE_NODE : LINKLET_WAL_UPDATE_EDGE;
                        operation.payload = &call->payload;
                    } else {
                        operation.operation =
                            is_node ? LINKLET_WAL_DELETE_NODE : LINKLET_WAL_DELETE_EDGE;
                        operation.detach = call->detach;
                    }
                    operation.id = ids.ids[match_index];
                    if (!append_operation(&operations, &operation_count, &capacity, operation)) {
                        success = false;
                        break;
                    }
                }
            }
            linklet_id_list_destroy(&ids);
        }
    }

    if (success) {
        char *path = wal_path(transaction->database);
        LinkletMmapFile *wal = path ? linklet_wal_open(path, false, error) : NULL;
        free(path);
        if (!wal) {
            success = false;
        } else {
            static uint64_t next_transaction_id = 1;
            success = linklet_wal_write_frame(wal, next_transaction_id++, operations,
                                              operation_count, error) &&
                      linklet_wal_apply(transaction->database->store, operations, operation_count,
                                        error) &&
                      linklet_wal_clear(wal, error);
            linklet_wal_close(wal);
        }
    }

    free(operations);
    clear_transaction(transaction);
    return success;
}

void linklet_transaction_abort(LinkletTransaction *transaction) {
    clear_transaction(transaction);
}
