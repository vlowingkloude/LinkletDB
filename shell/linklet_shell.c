#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "binder.h"
#include "database.h"
#include "gql_parser.h"
#include "transaction.h"

enum {
    LINKLET_SHELL_PROGRAM_ARGUMENT_INDEX = 0,
    LINKLET_SHELL_PATH_ARGUMENT_INDEX = 1,
    LINKLET_SHELL_GRAPH_ARGUMENT_INDEX = 2,
    LINKLET_SHELL_MINIMUM_ARGUMENT_COUNT = 2,
    LINKLET_SHELL_GRAPH_ARGUMENT_COUNT = 3,
    LINKLET_SHELL_USAGE_EXIT_CODE = 2,
    LINKLET_SHELL_INITIAL_EDGE_CAPACITY = 16,
    LINKLET_SHELL_LINE_CAPACITY = 4096,
};

static void trim(char *string) {
    char *first_non_whitespace = string;
    while (*first_non_whitespace == ' ' || *first_non_whitespace == '\t' ||
           *first_non_whitespace == '\n' || *first_non_whitespace == '\r') {
        first_non_whitespace++;
    }
    if (first_non_whitespace != string) {
        memmove(string, first_non_whitespace, strlen(first_non_whitespace) + 1);
    }
    size_t length = strlen(string);
    while (length > 0 && (string[length - 1] == ' ' || string[length - 1] == '\t' ||
                          string[length - 1] == '\n' || string[length - 1] == '\r')) {
        string[--length] = '\0';
    }
}

static int strings_equal_case_insensitive(const char *left, const char *right) {
    return strcasecmp(left, right) == 0;
}

static void print_result(const LinkletResult *result) {
    switch (result->kind) {
    case LINKLET_RESULT_IDS:
        printf("[%zu %s]", result->ids.count,
               result->element_kind == LINKLET_ELEMENT_NODE ? "node" : "edge");
        for (size_t index = 0; index < result->ids.count; ++index) {
            printf(" %llu", (unsigned long long)result->ids.ids[index]);
        }
        printf("\n");
        break;
    case LINKLET_RESULT_BOOL:
        printf("%s\n", result->boolean ? "true" : "false");
        break;
    case LINKLET_RESULT_COUNT:
        printf("affected %zu row(s), inserted id %llu\n", result->affected_count,
               (unsigned long long)result->inserted_id);
        break;
    }
}

int main(const int argc, char **argv) {
    if (argc < LINKLET_SHELL_MINIMUM_ARGUMENT_COUNT) {
        fprintf(stderr, "usage: %s <path> [graph_name]\n",
                argv[LINKLET_SHELL_PROGRAM_ARGUMENT_INDEX]);
        return LINKLET_SHELL_USAGE_EXIT_CODE;
    }
    const char *path = argv[LINKLET_SHELL_PATH_ARGUMENT_INDEX];
    const char *name = argc >= LINKLET_SHELL_GRAPH_ARGUMENT_COUNT
                           ? argv[LINKLET_SHELL_GRAPH_ARGUMENT_INDEX]
                           : NULL;

    LinkletError error;
    LinkletDatabase *database =
        name ? linklet_database_create(path, name, LINKLET_SHELL_INITIAL_EDGE_CAPACITY, &error)
             : linklet_database_open(path, &error);
    if (!database) {
        fprintf(stderr, "error: %s\n", error.message);
        return EXIT_FAILURE;
    }
    printf("linklet graph '%s' (%s)\n", linklet_database_catalog(database)->name,
           name ? "created" : "opened");

    LinkletTransaction transaction = {0};
    char line[LINKLET_SHELL_LINE_CAPACITY];
    while (printf("linklet> "), fflush(stdout), fgets(line, sizeof(line), stdin)) {
        trim(line);
        if (!line[0]) {
            continue;
        }
        if (strings_equal_case_insensitive(line, "exit") ||
            strings_equal_case_insensitive(line, "quit")) {
            break;
        }
        if (strings_equal_case_insensitive(line, "begin") ||
            strings_equal_case_insensitive(line, "start transaction")) {
            if (transaction.active) {
                printf("already in a transaction\n");
                continue;
            }
            printf(linklet_database_begin(database, &transaction, &error) ? "begin\n"
                                                                          : "error: %s\n",
                   error.message);
            continue;
        }
        if (strings_equal_case_insensitive(line, "commit")) {
            if (!transaction.active) {
                printf("not in a transaction\n");
                continue;
            }
            printf(linklet_transaction_commit(&transaction, &error) ? "committed\n" : "error: %s\n",
                   error.message);
            continue;
        }
        if (strings_equal_case_insensitive(line, "rollback") ||
            strings_equal_case_insensitive(line, "abort")) {
            if (!transaction.active) {
                printf("not in a transaction\n");
                continue;
            }
            linklet_transaction_abort(&transaction);
            printf("rolled back\n");
            continue;
        }

#ifdef LINKLET_DEBUG
        if (line[0] == '?') {
            GqlParseError parse_error;
            GqlNode *ast = gql_parse_program(line + 1, &parse_error);
            if (!ast) {
                printf("parse error: %s\n", parse_error.message);
                continue;
            }
            LinkletLogicalPlan plan = {0};
            if (!linklet_bind(ast, &plan, &error)) {
                printf("bind error: %s\n", error.message);
            } else {
                linklet_logical_plan_dump(&plan, stdout);
            }
            linklet_logical_plan_destroy(&plan);
            gql_node_free(ast);
            continue;
        }
#else
        if (line[0] == '?') {
            printf("'?' plan dump is disabled (build with LINKLET_DEBUG)\n");
            continue;
        }
#endif

        LinkletResult result = {0};
        const bool ok = transaction.active
                            ? linklet_transaction_execute(&transaction, line, &error)
                            : linklet_database_execute(database, line, &result, &error);
        if (!ok) {
            printf("error: %s\n", error.message);
        } else if (transaction.active) {
            printf("buffered\n");
        } else {
            print_result(&result);
        }
        linklet_result_destroy(&result);
    }

    if (transaction.active) {
        linklet_transaction_abort(&transaction);
    }
    linklet_database_close(database, &error);
    printf("bye\n");
    return EXIT_SUCCESS;
}
