#include "binder.h"

#include "bson_ast.h"
#include "bson_reader.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    LINKLET_NODE_PATH_ELEMENT_COUNT = 1,
    LINKLET_EDGE_PATH_ELEMENT_COUNT = 3,
    LINKLET_PATH_EDGE_INDEX = 1,
    LINKLET_PATH_DESTINATION_INDEX = 2,
    LINKLET_EDGE_ENDPOINT_COUNT = 2,
    LINKLET_BINDING_AND_PROPERTY_CHILD_COUNT = 2,
    LINKLET_QUANTIFIER_BOUND_COUNT = 2,
    LINKLET_SET_ITEM_CHILD_COUNT = 3,
    LINKLET_SET_ITEM_PROPERTY_INDEX = 1,
    LINKLET_SET_ITEM_VALUE_INDEX = 2,
    LINKLET_MINIMUM_REACHABILITY_HOPS = 1,
    LINKLET_UNBOUNDED_REACHABILITY_HOPS = 0,
};

typedef struct MatchBinding {
    const char *name;
    uint64_t node_id;
} MatchBinding;

static void set_error(LinkletError *error, const char *format, ...) {
    if (!error) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
}

static char *duplicate(const char *text) {
    if (!text) {
        return NULL;
    }
    const size_t size = strlen(text) + 1;
    char *copy = (char *)malloc(size);
    if (copy) {
        memcpy(copy, text, size);
    }
    return copy;
}

static const GqlNode *find_first(const GqlNode *node, const GqlNodeKind kind) {
    if (!node) {
        return NULL;
    }
    if (node->kind == kind) {
        return node;
    }
    for (size_t child = 0; child < node->child_count; ++child) {
        const GqlNode *found = find_first(node->children[child], kind);
        if (found) {
            return found;
        }
    }
    return NULL;
}

static const GqlNode *direct_child(const GqlNode *node, const GqlNodeKind kind) {
    if (!node) {
        return NULL;
    }
    for (size_t child = 0; child < node->child_count; ++child) {
        if (node->children[child]->kind == kind) {
            return node->children[child];
        }
    }
    return NULL;
}

static const GqlNode *find_match_path(const GqlNode *statement, const GqlNode **match_out) {
    const GqlNode *match = direct_child(statement, GQL_MATCH);
    if (match_out) {
        *match_out = match;
    }
    const GqlNode *graph = direct_child(match, GQL_GRAPH_PATTERN);
    if (!graph || graph->child_count != 1 || graph->children[0]->kind != GQL_PATH_PATTERN ||
        graph->children[0]->child_count != 1) {
        return NULL;
    }
    return direct_child(graph->children[0], GQL_PATH_EXPRESSION);
}

static bool read_one_label(const GqlNode *node, char **label, LinkletError *error) {
    if (!node) {
        return true;
    }
    if (node->kind == GQL_NAME && node->text) {
        if (*label) {
            set_error(error, "the initial matcher supports one label per element");
            return false;
        }
        *label = duplicate(node->text);
        return *label != NULL;
    }
    if (node->kind != GQL_LABEL_EXPRESSION ||
        (node->subkind != GQL_LABEL_CONJUNCTION && node->subkind != GQL_LABEL_NAME)) {
        set_error(error, "MATCH supports a simple label predicate");
        return false;
    }
    for (size_t child = 0; child < node->child_count; ++child) {
        if (!read_one_label(node->children[child], label, error)) {
            return false;
        }
    }
    return true;
}

static bool read_filter(const GqlNode *element, const char **name, LinkletObjectFilter *filter,
                        LinkletError *error) {
    const GqlNode *filler = direct_child(element, GQL_ELEMENT_FILLER);
    if (!filler) {
        set_error(error, "MATCH graph elements require an element filler");
        return false;
    }
    const GqlNode *name_node = direct_child(filler, GQL_NAME);
    if (!name_node || !name_node->text) {
        set_error(error, "MATCH graph elements require a binding name");
        return false;
    }
    *name = name_node->text;
    if (!read_one_label(direct_child(filler, GQL_LABEL_EXPRESSION), &filter->label, error)) {
        return false;
    }
    linklet_bson_init(&filter->properties);
    for (size_t child = 0; child < filler->child_count; ++child) {
        const GqlNode *property = filler->children[child];
        if (property->kind != GQL_PROPERTY_SPEC) {
            continue;
        }
        if (!property->text || property->child_count != 1 ||
            property->children[0]->kind != GQL_LITERAL) {
            set_error(error, "MATCH properties must compare a key with one literal");
            return false;
        }
        if (strcmp(property->text, "id") == 0 || strcmp(property->text, "_id") == 0) {
            if (property->children[0]->subkind != GQL_LIT_INTEGER || filter->has_id) {
                set_error(error, "element ID predicate must be one non-negative integer");
                return false;
            }
            filter->has_id = true;
            filter->id = property->children[0]->unsigned_integer_value;
        } else if (linklet_bson_has_field(&filter->properties, property->text)) {
            set_error(error, "duplicate MATCH property '%s'", property->text);
            return false;
        } else if (!linklet_bson_append_gql_literal(
                       &filter->properties, property->text, property->children[0],
                       error ? error->message : NULL, error ? sizeof(error->message) : 0)) {
            return false;
        }
    }
    return true;
}

static bool bind_match_filters(const GqlNode *ast, const char **source_name, const char **edge_name,
                               const char **destination_name, LinkletKernelCall *call,
                               LinkletError *error) {
    const GqlNode *statement = find_first(ast, GQL_STATEMENT);
    const GqlNode *path = find_match_path(statement, NULL);
    if (!path ||
        (path->child_count != LINKLET_NODE_PATH_ELEMENT_COUNT &&
         path->child_count != LINKLET_EDGE_PATH_ELEMENT_COUNT) ||
        path->children[0]->kind != GQL_NODE_PATTERN) {
        set_error(error, "MATCH supports one node or one node-edge-node pattern");
        return false;
    }
    if (!read_filter(path->children[0], source_name, &call->match.source, error)) {
        return false;
    }
    if (path->child_count == LINKLET_EDGE_PATH_ELEMENT_COUNT) {
        const GqlNode *edge = path->children[LINKLET_PATH_EDGE_INDEX];
        const GqlNode *destination = path->children[LINKLET_PATH_DESTINATION_INDEX];
        if (edge->kind != GQL_EDGE_PATTERN || destination->kind != GQL_NODE_PATTERN ||
            (edge->subkind != GQL_DIR_RIGHT && edge->subkind != GQL_DIR_LEFT)) {
            set_error(error, "edge MATCH currently supports directed edges only");
            return false;
        }
        call->match.is_edge_pattern = true;
        call->direction = edge->subkind == GQL_DIR_RIGHT ? LINKLET_DIR_RIGHT : LINKLET_DIR_LEFT;
        if (!read_filter(edge, edge_name, &call->match.edge, error) ||
            !read_filter(destination, destination_name, &call->match.destination, error)) {
            return false;
        }
    }
    return true;
}

static bool select_result(LinkletKernelCall *call, const char *source_name, const char *edge_name,
                          const char *destination_name, const char *target, LinkletError *error) {
    if (strcmp(target, source_name) == 0) {
        call->match.result_kind = LINKLET_ELEMENT_NODE;
        call->match.result_binding = LINKLET_MATCH_SOURCE;
    } else if (call->match.is_edge_pattern && strcmp(target, edge_name) == 0) {
        call->match.result_kind = LINKLET_ELEMENT_EDGE;
        call->match.result_binding = LINKLET_MATCH_EDGE;
    } else if (call->match.is_edge_pattern && strcmp(target, destination_name) == 0) {
        call->match.result_kind = LINKLET_ELEMENT_NODE;
        call->match.result_binding = LINKLET_MATCH_DESTINATION;
    } else {
        set_error(error, "binding '%s' is not defined by MATCH", target ? target : "");
        return false;
    }
    return true;
}

static bool read_named_node_with_id(const GqlNode *pattern, const char **name, uint64_t *id,
                                    LinkletError *error) {
    const GqlNode *filler = direct_child(pattern, GQL_ELEMENT_FILLER);
    if (!filler || filler->child_count != LINKLET_BINDING_AND_PROPERTY_CHILD_COUNT) {
        set_error(error, "reachability endpoints require one binding and one integer id property");
        return false;
    }
    const GqlNode *binding = direct_child(filler, GQL_NAME);
    const GqlNode *property = direct_child(filler, GQL_PROPERTY_SPEC);
    if (!binding || !binding->text || !property || !property->text ||
        strcmp(property->text, "id") != 0 || property->child_count != 1 ||
        property->children[0]->kind != GQL_LITERAL ||
        property->children[0]->subkind != GQL_LIT_INTEGER) {
        set_error(error, "reachability endpoints must use the form (name {id: <integer>})");
        return false;
    }
    *name = binding->text;
    *id = property->children[0]->unsigned_integer_value;
    return true;
}

static bool read_bounded_edge(const GqlNode *pattern, const char **name, size_t *max_hops,
                              LinkletError *error) {
    const GqlNode *filler = direct_child(pattern, GQL_ELEMENT_FILLER);
    const GqlNode *quantifier = direct_child(pattern, GQL_QUANTIFIER);
    if (!filler || filler->child_count != 1 || !quantifier ||
        quantifier->subkind != GQL_QUANT_GENERAL ||
        quantifier->child_count != LINKLET_QUANTIFIER_BOUND_COUNT) {
        set_error(error,
                  "bounded reachability requires a named edge with an explicit {1,max} quantifier");
        return false;
    }
    const GqlNode *binding = direct_child(filler, GQL_NAME);
    const GqlNode *lower = quantifier->children[0];
    const GqlNode *upper = quantifier->children[1];
    if (!binding || !binding->text || lower->kind != GQL_LITERAL || upper->kind != GQL_LITERAL ||
        lower->subkind != GQL_LIT_INTEGER || upper->subkind != GQL_LIT_INTEGER ||
        lower->unsigned_integer_value != LINKLET_MINIMUM_REACHABILITY_HOPS ||
        upper->unsigned_integer_value == LINKLET_UNBOUNDED_REACHABILITY_HOPS ||
        upper->unsigned_integer_value > SIZE_MAX) {
        set_error(error,
                  "the initial reachability kernel supports bounded paths of the form {1,max}");
        return false;
    }
    *name = binding->text;
    *max_hops = (size_t)upper->unsigned_integer_value;
    return true;
}

static bool bind_reachability(const GqlNode *ast, const GqlNode *path, LinkletKernelCall *call,
                              LinkletError *error) {
    const GqlNode *statement = find_first(ast, GQL_STATEMENT);
    const GqlNode *return_clause = direct_child(statement, GQL_RETURN);
    if (!return_clause || return_clause->child_count != 1 ||
        return_clause->children[0]->kind != GQL_BINDING_VAR || !return_clause->children[0]->text) {
        set_error(error, "reachability requires RETURN <destination binding>");
        return false;
    }
    const GqlNode *edge = path->children[LINKLET_PATH_EDGE_INDEX];
    if (edge->subkind != GQL_DIR_RIGHT && edge->subkind != GQL_DIR_LEFT) {
        set_error(error, "the initial reachability kernel supports directed paths only");
        return false;
    }
    const char *source_name = NULL;
    const char *edge_name = NULL;
    const char *destination_name = NULL;
    uint64_t source_id = 0;
    uint64_t destination_id = 0;
    size_t max_hops = 0;
    if (!read_named_node_with_id(path->children[0], &source_name, &source_id, error) ||
        !read_bounded_edge(edge, &edge_name, &max_hops, error) ||
        !read_named_node_with_id(path->children[LINKLET_PATH_DESTINATION_INDEX], &destination_name,
                                 &destination_id, error)) {
        return false;
    }
    if (strcmp(return_clause->children[0]->text, destination_name) != 0) {
        set_error(error,
                  "bounded reachability currently returns the constrained destination binding");
        return false;
    }
    call->code = LINKLET_KERNEL_REACHABILITY;
    call->direction = edge->subkind == GQL_DIR_RIGHT ? LINKLET_DIR_RIGHT : LINKLET_DIR_LEFT;
    call->source_id = source_id;
    call->destination_id = destination_id;
    call->max_hops = max_hops;
    return true;
}

static bool bind_match(const GqlNode *ast, LinkletKernelCall *call, LinkletError *error) {
    const GqlNode *statement = find_first(ast, GQL_STATEMENT);
    const GqlNode *path = find_match_path(statement, NULL);
    if (!path ||
        (path->child_count != LINKLET_NODE_PATH_ELEMENT_COUNT &&
         path->child_count != LINKLET_EDGE_PATH_ELEMENT_COUNT) ||
        path->children[0]->kind != GQL_NODE_PATTERN) {
        set_error(error, "MATCH supports one node or one node-edge-node pattern");
        return false;
    }

    if (path->child_count == LINKLET_EDGE_PATH_ELEMENT_COUNT &&
        path->children[LINKLET_PATH_EDGE_INDEX]->kind == GQL_EDGE_PATTERN &&
        direct_child(path->children[LINKLET_PATH_EDGE_INDEX], GQL_QUANTIFIER)) {
        return bind_reachability(ast, path, call, error);
    }

    const char *source_name = NULL;
    const char *edge_name = NULL;
    const char *destination_name = NULL;
    call->code = LINKLET_KERNEL_MATCH;
    if (!bind_match_filters(ast, &source_name, &edge_name, &destination_name, call, error)) {
        return false;
    }
    const GqlNode *return_clause = direct_child(statement, GQL_RETURN);
    if (!return_clause || return_clause->child_count != 1 ||
        return_clause->children[0]->kind != GQL_BINDING_VAR || !return_clause->children[0]->text) {
        set_error(error, "MATCH query requires RETURN <one binding>");
        return false;
    }
    return select_result(call, source_name, edge_name, destination_name,
                         return_clause->children[0]->text, error);
}

static bool bind_insert_node(const GqlNode *insert, LinkletKernelCall *call, LinkletError *error) {
    const GqlNode *graph = direct_child(insert, GQL_INSERT_GRAPH_PATTERN);
    if (!graph || graph->child_count != 1 || graph->children[0]->kind != GQL_INSERT_PATH_PATTERN ||
        graph->children[0]->child_count != 1 ||
        graph->children[0]->children[0]->kind != GQL_INSERT_NODE_PATTERN) {
        set_error(error, "node insertion supports exactly one node pattern");
        return false;
    }
    return linklet_encode_element_bson(graph->children[0]->children[0], &call->payload,
                                       error ? error->message : NULL,
                                       error ? sizeof(error->message) : 0);
}

static bool read_match_binding(const GqlNode *path, MatchBinding *binding, LinkletError *error) {
    if (!path || path->kind != GQL_PATH_PATTERN || path->child_count != 1) {
        set_error(error, "edge insertion MATCH supports simple node bindings only");
        return false;
    }
    const GqlNode *expression = direct_child(path, GQL_PATH_EXPRESSION);
    if (!expression || expression->child_count != 1 ||
        expression->children[0]->kind != GQL_NODE_PATTERN) {
        set_error(error, "edge insertion MATCH must bind individual nodes");
        return false;
    }
    const GqlNode *filler = direct_child(expression->children[0], GQL_ELEMENT_FILLER);
    const GqlNode *name = direct_child(filler, GQL_NAME);
    const GqlNode *property = direct_child(filler, GQL_PROPERTY_SPEC);
    if (!filler || filler->child_count != LINKLET_BINDING_AND_PROPERTY_CHILD_COUNT || !name ||
        !name->text || !property || !property->text ||
        (strcmp(property->text, "id") != 0 && strcmp(property->text, "_id") != 0) ||
        property->child_count != 1 || property->children[0]->kind != GQL_LITERAL ||
        property->children[0]->subkind != GQL_LIT_INTEGER) {
        set_error(error, "edge insertion MATCH bindings require one integer id property");
        return false;
    }
    binding->name = name->text;
    binding->node_id = property->children[0]->unsigned_integer_value;
    return true;
}

static bool resolve_binding(const MatchBinding bindings[LINKLET_EDGE_ENDPOINT_COUNT],
                            const char *name, uint64_t *node_id, LinkletError *error) {
    for (size_t index = 0; index < LINKLET_EDGE_ENDPOINT_COUNT; ++index) {
        if (strcmp(bindings[index].name, name) == 0) {
            *node_id = bindings[index].node_id;
            return true;
        }
    }
    set_error(error, "insert endpoint '%s' is not bound by MATCH", name ? name : "");
    return false;
}

static bool bind_insert_edge(const GqlNode *match, const GqlNode *insert, LinkletKernelCall *call,
                             LinkletError *error) {
    const GqlNode *match_graph = direct_child(match, GQL_GRAPH_PATTERN);
    if (!match_graph || match_graph->child_count != LINKLET_EDGE_ENDPOINT_COUNT) {
        set_error(error, "edge insertion requires two MATCH node bindings");
        return false;
    }
    MatchBinding bindings[LINKLET_EDGE_ENDPOINT_COUNT];
    if (!read_match_binding(match_graph->children[0], &bindings[0], error) ||
        !read_match_binding(match_graph->children[1], &bindings[1], error) ||
        strcmp(bindings[0].name, bindings[1].name) == 0) {
        if (error && !error->message[0]) {
            set_error(error, "MATCH node bindings must have distinct names");
        }
        return false;
    }
    const GqlNode *insert_graph = direct_child(insert, GQL_INSERT_GRAPH_PATTERN);
    if (!insert_graph || insert_graph->child_count != 1 ||
        insert_graph->children[0]->kind != GQL_INSERT_PATH_PATTERN ||
        insert_graph->children[0]->child_count != 3 ||
        insert_graph->children[0]->children[0]->kind != GQL_INSERT_NODE_PATTERN ||
        insert_graph->children[0]->children[1]->kind != GQL_INSERT_EDGE_PATTERN ||
        insert_graph->children[0]->children[2]->kind != GQL_INSERT_NODE_PATTERN) {
        set_error(error, "edge insertion supports one node-edge-node pattern");
        return false;
    }
    const GqlNode *path = insert_graph->children[0];
    const GqlNode *source_filler = direct_child(path->children[0], GQL_ELEMENT_FILLER);
    const GqlNode *destination_filler =
        direct_child(path->children[LINKLET_PATH_DESTINATION_INDEX], GQL_ELEMENT_FILLER);
    const GqlNode *source_name = direct_child(source_filler, GQL_NAME);
    const GqlNode *destination_name = direct_child(destination_filler, GQL_NAME);
    const GqlNode *edge = path->children[LINKLET_PATH_EDGE_INDEX];
    if (!source_filler || source_filler->child_count != 1 || !source_name || !source_name->text ||
        !destination_filler || destination_filler->child_count != 1 || !destination_name ||
        !destination_name->text ||
        (edge->subkind != GQL_DIR_RIGHT && edge->subkind != GQL_DIR_LEFT)) {
        set_error(error, "insert endpoints must reference MATCH bindings and use a directed edge");
        return false;
    }
    uint64_t left = 0;
    uint64_t right = 0;
    if (!resolve_binding(bindings, source_name->text, &left, error) ||
        !resolve_binding(bindings, destination_name->text, &right, error)) {
        return false;
    }
    call->insert_kind = LINKLET_ELEMENT_EDGE;
    call->insert_source_id = edge->subkind == GQL_DIR_RIGHT ? left : right;
    call->insert_destination_id = edge->subkind == GQL_DIR_RIGHT ? right : left;
    return linklet_encode_element_bson(edge, &call->payload, error ? error->message : NULL,
                                       error ? sizeof(error->message) : 0);
}

static bool bind_update(const GqlNode *ast, const GqlNode *set, LinkletKernelCall *call,
                        LinkletError *error) {
    const char *source_name = NULL;
    const char *edge_name = NULL;
    const char *destination_name = NULL;
    call->code = LINKLET_KERNEL_UPDATE;
    if (!bind_match_filters(ast, &source_name, &edge_name, &destination_name, call, error)) {
        return false;
    }
    if (!set || set->child_count == 0) {
        set_error(error, "SET requires at least one assignment");
        return false;
    }
    linklet_bson_init(&call->payload);
    const char *target = NULL;
    for (size_t index = 0; index < set->child_count; ++index) {
        const GqlNode *item = set->children[index];
        if (item->kind != GQL_SET_ITEM || item->subkind != GQL_SET_PROPERTY ||
            item->child_count != LINKLET_SET_ITEM_CHILD_COUNT ||
            item->children[0]->kind != GQL_BINDING_VAR || !item->children[0]->text ||
            item->children[LINKLET_SET_ITEM_PROPERTY_INDEX]->kind != GQL_NAME ||
            !item->children[LINKLET_SET_ITEM_PROPERTY_INDEX]->text) {
            set_error(error, "SET currently supports binding.property = literal");
            return false;
        }
        if (!target) {
            target = item->children[0]->text;
        }
        if (strcmp(target, item->children[0]->text) != 0) {
            set_error(error, "one SET statement must update one binding");
            return false;
        }
        if (strcmp(item->children[LINKLET_SET_ITEM_PROPERTY_INDEX]->text, "id") == 0 ||
            strcmp(item->children[LINKLET_SET_ITEM_PROPERTY_INDEX]->text, "_id") == 0 ||
            strcmp(item->children[LINKLET_SET_ITEM_PROPERTY_INDEX]->text, "_labels") == 0) {
            set_error(error, "ID and label metadata cannot be changed through SET");
            return false;
        }
        if (linklet_bson_has_field(&call->payload,
                                   item->children[LINKLET_SET_ITEM_PROPERTY_INDEX]->text)) {
            set_error(error, "property '%s' is assigned more than once",
                      item->children[LINKLET_SET_ITEM_PROPERTY_INDEX]->text);
            return false;
        }
        if (!linklet_bson_append_gql_literal(
                &call->payload, item->children[LINKLET_SET_ITEM_PROPERTY_INDEX]->text,
                item->children[LINKLET_SET_ITEM_VALUE_INDEX], error ? error->message : NULL,
                error ? sizeof(error->message) : 0)) {
            return false;
        }
    }
    return select_result(call, source_name, edge_name, destination_name, target, error);
}

static bool bind_delete(const GqlNode *ast, const GqlNode *delete_statement,
                        LinkletKernelCall *call, LinkletError *error) {
    const char *source_name = NULL;
    const char *edge_name = NULL;
    const char *destination_name = NULL;
    call->code = LINKLET_KERNEL_DELETE;
    if (!bind_match_filters(ast, &source_name, &edge_name, &destination_name, call, error)) {
        return false;
    }
    if (!delete_statement || delete_statement->child_count != 1 ||
        delete_statement->children[0]->kind != GQL_BINDING_VAR ||
        !delete_statement->children[0]->text ||
        !select_result(call, source_name, edge_name, destination_name,
                       delete_statement->children[0]->text, error)) {
        if (error && !error->message[0]) {
            set_error(error, "DELETE supports one matched binding");
        }
        return false;
    }
    call->detach = delete_statement->subkind == GQL_DELETE_DETACH;
    if (call->detach && call->match.result_kind != LINKLET_ELEMENT_NODE) {
        set_error(error, "DETACH DELETE applies to nodes");
        return false;
    }
    return true;
}

bool linklet_bind(const GqlNode *ast, LinkletLogicalPlan *plan, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!ast || !plan) {
        set_error(error, "AST and logical plan are required");
        return false;
    }
    *plan = (LinkletLogicalPlan){0};

    const GqlNode *statement = find_first(ast, GQL_STATEMENT);
    const GqlNode *match = direct_child(statement, GQL_MATCH);
    const GqlNode *insert = direct_child(statement, GQL_INSERT);
    const GqlNode *set_statement = direct_child(statement, GQL_SET_STATEMENT);
    const GqlNode *delete_statement = direct_child(statement, GQL_DELETE_STATEMENT);

    LinkletKernelCall call = {0};

    bool success;
    if (insert && !match) {
        call.code = LINKLET_KERNEL_INSERT;
        call.insert_kind = LINKLET_ELEMENT_NODE;
        success = bind_insert_node(insert, &call, error);
    } else if (insert && match) {
        call.code = LINKLET_KERNEL_INSERT;
        success = bind_insert_edge(match, insert, &call, error);
    } else if (match && set_statement) {
        success = bind_update(ast, set_statement, &call, error);
    } else if (match && delete_statement) {
        success = bind_delete(ast, delete_statement, &call, error);
    } else if (match) {
        success = bind_match(ast, &call, error);
    } else {
        set_error(error, "expected INSERT, MATCH, MATCH ... SET, or MATCH ... DELETE");
        success = false;
    }

    if (!success) {
        linklet_kernel_call_destroy(&call);
        return false;
    }

    plan->calls = (LinkletKernelCall *)calloc(1, sizeof(LinkletKernelCall));
    if (!plan->calls) {
        set_error(error, "out of memory allocating the logical plan");
        linklet_kernel_call_destroy(&call);
        return false;
    }
    plan->calls[0] = call;
    plan->call_count = 1;
    return true;
}
