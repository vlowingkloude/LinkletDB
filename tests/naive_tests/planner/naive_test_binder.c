#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "binder.h"
#include "bson_reader.h"
#include "gql_parser.h"
#include "naive_tests.h"

static LinkletLogicalPlan bind_ok(const char *source) {
    GqlParseError parse_error;
    GqlNode *ast = gql_parse_program(source, &parse_error);
    if (!ast) {
        return (LinkletLogicalPlan){0};
    }
    LinkletLogicalPlan plan = {0};
    LinkletError error;
    if (!linklet_bind(ast, &plan, &error)) {
        gql_node_free(ast);
        return (LinkletLogicalPlan){0};
    }
    gql_node_free(ast);
    return plan;
}

static bool bind_rejects(const char *source, const char *needle) {
    GqlParseError parse_error;
    GqlNode *ast = gql_parse_program(source, &parse_error);
    if (!ast) {
        return false;
    }
    LinkletLogicalPlan plan = {0};
    LinkletError error;
    const bool rejected = !linklet_bind(ast, &plan, &error) &&
                          (needle == NULL || strstr(error.message, needle) != NULL);
    linklet_logical_plan_destroy(&plan);
    gql_node_free(ast);
    return rejected;
}

bool test_bind_match_node_pattern(void) {
    LinkletLogicalPlan plan = bind_ok("MATCH (n:User {name: 'Alice'}) RETURN n");
    if (!plan.calls) {
        return false;
    }
    const LinkletKernelCall *call = &plan.calls[0];
    bool ok = call->code == LINKLET_KERNEL_MATCH && !call->match.is_edge_pattern &&
              call->match.result_kind == LINKLET_ELEMENT_NODE &&
              call->match.result_binding == LINKLET_MATCH_SOURCE && call->match.source.label &&
              strcmp(call->match.source.label, "User") == 0;
    const char *name = NULL;
    size_t name_len = 0;
    ok = ok && linklet_bson_get_utf8(&call->match.source.properties, "name", &name, &name_len) &&
         name_len == 5 && memcmp(name, "Alice", 5) == 0;
    linklet_logical_plan_destroy(&plan);
    return ok;
}

bool test_bind_match_edge_pattern(void) {
    LinkletLogicalPlan plan = bind_ok("MATCH (a)-[e:KNOWS]->(b) RETURN e");
    if (!plan.calls) {
        return false;
    }
    const LinkletKernelCall *call = &plan.calls[0];
    const bool ok = call->code == LINKLET_KERNEL_MATCH && call->match.is_edge_pattern &&
                    call->direction == LINKLET_DIR_RIGHT &&
                    call->match.result_kind == LINKLET_ELEMENT_EDGE &&
                    call->match.result_binding == LINKLET_MATCH_EDGE && call->match.edge.label &&
                    strcmp(call->match.edge.label, "KNOWS") == 0;
    linklet_logical_plan_destroy(&plan);
    return ok;
}

bool test_bind_reachability(void) {
    LinkletLogicalPlan plan = bind_ok("MATCH (a {id: 1})-[e]->{1,3}(b {id: 4}) RETURN b");
    if (!plan.calls) {
        return false;
    }
    const LinkletKernelCall *call = &plan.calls[0];
    const bool ok = call->code == LINKLET_KERNEL_REACHABILITY &&
                    call->direction == LINKLET_DIR_RIGHT && call->source_id == 1 &&
                    call->destination_id == 4 && call->max_hops == 3;
    linklet_logical_plan_destroy(&plan);
    return ok;
}

bool test_bind_insert_update_delete(void) {
    LinkletLogicalPlan plan = bind_ok("INSERT (a:User {name: 'Alice'})");
    bool ok = plan.calls && plan.calls[0].code == LINKLET_KERNEL_INSERT &&
              plan.calls[0].insert_kind == LINKLET_ELEMENT_NODE;
    linklet_logical_plan_destroy(&plan);

    plan = bind_ok("MATCH (a {id: 0}), (b {id: 1}) INSERT (a)-[e:KNOWS]->(b)");
    ok = ok && plan.calls && plan.calls[0].code == LINKLET_KERNEL_INSERT &&
         plan.calls[0].insert_kind == LINKLET_ELEMENT_EDGE && plan.calls[0].insert_source_id == 0 &&
         plan.calls[0].insert_destination_id == 1;
    linklet_logical_plan_destroy(&plan);

    plan = bind_ok("MATCH (n:User {name: 'Alice'}) SET n.age = 31 RETURN n");
    ok = ok && plan.calls && plan.calls[0].code == LINKLET_KERNEL_UPDATE &&
         plan.calls[0].match.result_kind == LINKLET_ELEMENT_NODE;
    int32_t age = 0;
    ok = ok && linklet_bson_get_int32(&plan.calls[0].payload, "age", &age) && age == 31;
    linklet_logical_plan_destroy(&plan);

    plan = bind_ok("MATCH (n {id: 1}) DETACH DELETE n");
    ok = ok && plan.calls && plan.calls[0].code == LINKLET_KERNEL_DELETE && plan.calls[0].detach;
    linklet_logical_plan_destroy(&plan);
    return ok;
}

bool test_bind_rejects_undefined_return_binding(void) {
    return bind_rejects("MATCH (a)-[e]->(b) RETURN missing", "not defined");
}
