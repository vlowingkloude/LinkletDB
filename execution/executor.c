#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "advisor.h"
#include "bson_reader.h"
#include "bson_writer.h"
#include "operators.h"
#include "executor.h"

static void set_error(LinkletError *error, const char *message) {
    if (error) {
        snprintf(error->message, sizeof(error->message), "%s", message ? message : "");
    }
}

static bool has_label(const LinkletBson *object, const char *label) {
    if (!label) {
        return true;
    }
    const uint8_t *data = NULL;
    size_t size = 0;
    if (!linklet_bson_get_array(object, "_labels", &data, &size)) {
        return false;
    }
    const LinkletBson labels = linklet_bson_view(data, size);
    LinkletBsonIterator iterator;
    if (!linklet_bson_iterator_init(&iterator, &labels)) {
        return false;
    }
    do {
        const char *value = NULL;
        size_t value_size = 0;
        if (linklet_bson_iterator_utf8(&iterator, &value, &value_size) &&
            strlen(label) == value_size && memcmp(value, label, value_size) == 0) {
            return true;
        }
    } while (linklet_bson_iterator_next(&iterator));
    return false;
}

static bool value_equal(const LinkletBsonIterator *left, const LinkletBsonIterator *right) {
    if (!left || !right || left->type != right->type) {
        return false;
    }
    const size_t left_size = left->next_offset - (size_t)(left->value - left->data);
    const size_t right_size = right->next_offset - (size_t)(right->value - right->data);
    return left_size == right_size && memcmp(left->value, right->value, left_size) == 0;
}

static bool matches_properties(const LinkletBson *object, const LinkletBson *properties) {
    if (!properties || linklet_bson_empty(properties)) {
        return true;
    }
    LinkletBsonIterator expected;
    if (!linklet_bson_iterator_init(&expected, properties)) {
        return true;
    }
    do {
        LinkletBsonIterator actual;
        if (!linklet_bson_find(object, expected.key, &actual) || !value_equal(&actual, &expected)) {
            return false;
        }
    } while (linklet_bson_iterator_next(&expected));
    return true;
}

static bool element_exists(const LinkletGraphStore *store, const LinkletElementKind kind,
                           const uint64_t id) {
    return kind == LINKLET_ELEMENT_NODE ? linklet_graph_store_node_exists(store, id)
                                        : linklet_graph_store_edge_exists(store, id);
}

static bool read_element(const LinkletGraphStore *store, const LinkletElementKind kind,
                         const uint64_t id, LinkletStoredObject *object, LinkletError *error) {
    return kind == LINKLET_ELEMENT_NODE ? linklet_graph_store_read_node(store, id, object, error)
                                        : linklet_graph_store_read_edge(store, id, object, error);
}

static size_t element_capacity(const LinkletGraphStore *store, const LinkletElementKind kind) {
    return kind == LINKLET_ELEMENT_NODE ? linklet_graph_store_node_id_capacity(store)
                                        : linklet_graph_store_edge_id_capacity(store);
}

static bool scan_elements(const LinkletGraphStore *store, const LinkletElementKind kind,
                          const LinkletObjectFilter *filter, LinkletIdList *matches,
                          LinkletError *error) {
    const uint64_t capacity = element_capacity(store, kind);
    const uint64_t begin = filter && filter->has_id ? filter->id : 0;
    const uint64_t end = filter && filter->has_id ? filter->id + 1 : capacity;
    for (uint64_t id = begin; id < end && id < capacity; ++id) {
        if (!element_exists(store, kind, id)) {
            continue;
        }
        LinkletStoredObject object = {0};
        if (!read_element(store, kind, id, &object, error)) {
            linklet_id_list_destroy(matches);
            return false;
        }
        const bool matched = (!filter || has_label(&object.bson, filter->label)) &&
                             (!filter || matches_properties(&object.bson, &filter->properties));
        linklet_stored_object_destroy(&object);
        if (matched && !linklet_id_list_append(matches, id)) {
            set_error(error, "could not grow match result");
            linklet_id_list_destroy(matches);
            return false;
        }
    }
    return true;
}

static bool id_list_contains(const LinkletIdList *list, const uint64_t id) {
    for (size_t index = 0; index < list->count; ++index) {
        if (list->ids[index] == id) {
            return true;
        }
    }
    return false;
}

bool linklet_resolve_match(const LinkletGraphStore *store, const LinkletMatchPattern *pattern,
                           const LinkletDirection direction, LinkletIdList *ids,
                           LinkletError *error) {
    if (!pattern->is_edge_pattern) {
        return scan_elements(store, LINKLET_ELEMENT_NODE, &pattern->source, ids, error);
    }

    LinkletIdList sources = {0};
    LinkletIdList edges = {0};
    LinkletIdList destinations = {0};
    bool success =
        scan_elements(store, LINKLET_ELEMENT_NODE, &pattern->source, &sources, error) &&
        scan_elements(store, LINKLET_ELEMENT_EDGE, &pattern->edge, &edges, error) &&
        scan_elements(store, LINKLET_ELEMENT_NODE, &pattern->destination, &destinations, error);
    for (size_t index = 0; success && index < edges.count; ++index) {
        uint64_t physical_source = 0;
        uint64_t physical_destination = 0;
        if (!linklet_graph_store_edge_endpoints(store, edges.ids[index], &physical_source,
                                                &physical_destination)) {
            set_error(error, "matched edge has no connectivity record");
            success = false;
            break;
        }
        const uint64_t source =
            direction == LINKLET_DIR_RIGHT ? physical_source : physical_destination;
        const uint64_t destination =
            direction == LINKLET_DIR_RIGHT ? physical_destination : physical_source;
        if (!id_list_contains(&sources, source) || !id_list_contains(&destinations, destination)) {
            continue;
        }
        const uint64_t value =
            pattern->result_binding == LINKLET_MATCH_SOURCE
                ? source
                : (pattern->result_binding == LINKLET_MATCH_EDGE ? edges.ids[index] : destination);
        if (!linklet_id_list_append(ids, value)) {
            set_error(error, "could not grow match result");
            success = false;
        }
    }
    linklet_id_list_destroy(&sources);
    linklet_id_list_destroy(&edges);
    linklet_id_list_destroy(&destinations);
    if (!success) {
        linklet_id_list_destroy(ids);
    }
    return success;
}

static bool execute_match(const LinkletGraphStore *store, const LinkletKernelCall *call,
                          LinkletResult *result, LinkletError *error) {
    result->kind = LINKLET_RESULT_IDS;
    result->element_kind = call->match.result_kind;
    return linklet_resolve_match(store, &call->match, call->direction, &result->ids, error);
}

static bool execute_reachability(const LinkletGraphStore *store, const LinkletKernelCall *call,
                                 const LinkletFlatCoo *coo, const LinkletExecutionPlan *execution,
                                 LinkletResult *result, LinkletError *error) {
    (void)store;
    unsigned char *scratch =
        (unsigned char *)calloc(execution->scratch_capacity, LINKLET_REACHABILITY_FRONTIER_COUNT);
    if (!scratch) {
        set_error(error, "could not allocate reachability scratch buffers");
        return false;
    }
    result->kind = LINKLET_RESULT_BOOL;
    result->boolean = operator_is_reachable_flat_coo_bounded(
        coo, call->source_id, call->destination_id, call->max_hops,
        call->direction == LINKLET_DIR_LEFT, scratch, scratch + execution->scratch_capacity,
        execution->scratch_capacity);
    free(scratch);
    return true;
}

static bool execute_insert(LinkletGraphStore *store, const LinkletKernelCall *call,
                           LinkletResult *result, LinkletError *error) {
    result->kind = LINKLET_RESULT_COUNT;
    const bool success =
        call->insert_kind == LINKLET_ELEMENT_NODE
            ? linklet_graph_store_insert_node(store, &call->payload, &result->inserted_id, error)
            : linklet_graph_store_insert_edge(store, call->insert_source_id,
                                              call->insert_destination_id, &call->payload,
                                              &result->inserted_id, error);
    result->affected_count = success ? 1 : 0;
    return success;
}

static bool already_processed(const LinkletIdList *ids, const size_t current) {
    for (size_t index = 0; index < current; ++index) {
        if (ids->ids[index] == ids->ids[current]) {
            return true;
        }
    }
    return false;
}

static bool execute_mutation(LinkletGraphStore *store, const LinkletKernelCall *call,
                             LinkletResult *result, LinkletError *error) {
    result->kind = LINKLET_RESULT_COUNT;
    LinkletIdList matches = {0};
    if (!linklet_resolve_match(store, &call->match, call->direction, &matches, error)) {
        return false;
    }

    bool success = true;
    for (size_t index = 0; success && index < matches.count; ++index) {
        if (already_processed(&matches, index)) {
            continue;
        }
        const uint64_t id = matches.ids[index];
        if (call->code == LINKLET_KERNEL_DELETE) {
            success = call->match.result_kind == LINKLET_ELEMENT_NODE
                          ? linklet_graph_store_delete_node(store, id, call->detach, error)
                          : linklet_graph_store_delete_edge(store, id, error);
        } else {
            LinkletStoredObject object = {0};
            success = read_element(store, call->match.result_kind, id, &object, error);
            LinkletBson replacement;
            LinkletBsonError bson_error;
            if (success &&
                !linklet_bson_merge(&object.bson, &call->payload, &replacement, &bson_error)) {
                set_error(error, bson_error.message);
                success = false;
            }
            if (success) {
                success = call->match.result_kind == LINKLET_ELEMENT_NODE
                              ? linklet_graph_store_update_node(store, id, &replacement, error)
                              : linklet_graph_store_update_edge(store, id, &replacement, error);
                linklet_bson_destroy(&replacement);
            }
            linklet_stored_object_destroy(&object);
        }
        if (success) {
            result->affected_count++;
        }
    }
    linklet_id_list_destroy(&matches);
    if (!success) {
        linklet_result_destroy(result);
    }
    return success;
}

bool linklet_execute(const LinkletLogicalPlan *plan, LinkletGraphStore *store,
                     LinkletResult *result, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!plan || !store || !result) {
        set_error(error, "logical plan, graph store, and result are required");
        return false;
    }
    if (plan->call_count != 1) {
        set_error(error, "multi-call logical plans are not supported yet");
        return false;
    }
    *result = (LinkletResult){0};
    const LinkletKernelCall *call = &plan->calls[0];
    const LinkletFlatCoo coo = linklet_graph_store_coo_view(store);
    LinkletExecutionPlan execution;
    if (!linklet_advise(call, &coo, &execution, error)) {
        return false;
    }

    switch (call->code) {
    case LINKLET_KERNEL_MATCH:
        return execute_match(store, call, result, error);
    case LINKLET_KERNEL_REACHABILITY:
        return execute_reachability(store, call, &coo, &execution, result, error);
    case LINKLET_KERNEL_INSERT:
        return execute_insert(store, call, result, error);
    case LINKLET_KERNEL_UPDATE:
    case LINKLET_KERNEL_DELETE:
        return execute_mutation(store, call, result, error);
    default:
        set_error(error, "unknown kernel code");
        return false;
    }
}

void linklet_result_destroy(LinkletResult *result) {
    if (!result) {
        return;
    }
    linklet_id_list_destroy(&result->ids);
    *result = (LinkletResult){0};
}
