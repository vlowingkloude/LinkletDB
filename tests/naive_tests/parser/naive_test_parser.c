#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "naive_tests.h"
#include "gql_parser.h"

static const GqlNode *find_kind(const GqlNode *node, GqlNodeKind kind) {
    if (!node) {
        return NULL;
    }
    if (node->kind == kind) {
        return node;
    }
    for (size_t i = 0; i < node->child_count; i++) {
        const GqlNode *found = find_kind(node->children[i], kind);
        if (found) {
            return found;
        }
    }
    return NULL;
}

static bool has_kind(const GqlNode *node, GqlNodeKind kind) {
    return find_kind(node, kind) != NULL;
}

static GqlNode *parse_ok(const char *source, char *errbuf, size_t errlen) {
    GqlParseError err;
    GqlNode *ast = gql_parse_program(source, &err);
    if (!ast) {
        snprintf(errbuf, errlen, "parse failed: %s (line %zu col %zu)", err.message, err.line,
                 err.column);
    }
    return ast;
}

bool test_simple_parser_match_edge() {
    const char *query = "MATCH (a)-[e]->(b) RETURN e";
    GqlParseError err;
    GqlNode *ast = gql_parse_program(query, &err);
    if (!ast) {
        return false;
    }
    const bool valid =
        has_kind(ast, GQL_MATCH) && has_kind(ast, GQL_EDGE_PATTERN) && has_kind(ast, GQL_RETURN);
    gql_node_free(ast);
    return valid;
}

bool test_parser_create_graph_shape() {
    char msg[256];
    GqlNode *ast = parse_ok("CREATE GRAPH mygraph ANY", msg, sizeof(msg));
    if (!ast) {
        fprintf(stderr, "  %s\n", msg);
        return false;
    }

    const GqlNode *create = find_kind(ast, GQL_CREATE_GRAPH);
    if (!create) {
        fprintf(stderr, "  missing create_graph node\n");
        gql_node_free(ast);
        return false;
    }
    if (!create->child_count || !create->children[0]->text ||
        strcmp(create->children[0]->text, "mygraph") != 0) {
        fprintf(stderr, "  create_graph name mismatch\n");
        gql_node_free(ast);
        return false;
    }
    if (!has_kind(create, GQL_OPEN_GRAPH_TYPE)) {
        fprintf(stderr, "  missing open_graph_type (ANY)\n");
        gql_node_free(ast);
        return false;
    }
    gql_node_free(ast);

    ast = parse_ok("CREATE GRAPH g ::{(City :City {name STRING})}", msg, sizeof(msg));
    if (!ast) {
        fprintf(stderr, "  %s\n", msg);
        return false;
    }
    if (!has_kind(ast, GQL_NESTED_GRAPH_TYPE)) {
        fprintf(stderr, "  missing nested_graph_type\n");
        gql_node_free(ast);
        return false;
    }
    if (!has_kind(ast, GQL_TYPED)) {
        fprintf(stderr, "  missing typed marker\n");
        gql_node_free(ast);
        return false;
    }
    if (!has_kind(ast, GQL_PROPERTY_TYPE)) {
        fprintf(stderr, "  missing property_type\n");
        gql_node_free(ast);
        return false;
    }
    gql_node_free(ast);

    ast = parse_ok("CREATE SCHEMA /foo/myschema", msg, sizeof(msg));
    if (!ast) {
        fprintf(stderr, "  %s\n", msg);
        return false;
    }
    const GqlNode *schema = find_kind(ast, GQL_CREATE_SCHEMA);
    if (!schema || !schema->child_count || !schema->children[0]->text ||
        strcmp(schema->children[0]->text, "myschema") != 0) {
        fprintf(stderr, "  create_schema name mismatch\n");
        gql_node_free(ast);
        return false;
    }
    const GqlNode *parent = find_kind(schema->children[0], GQL_CATALOG_PARENT_REFERENCE);
    if (!parent || !parent->text || strcmp(parent->text, "/foo/") != 0) {
        fprintf(stderr, "  create_schema parent path mismatch\n");
        gql_node_free(ast);
        return false;
    }
    gql_node_free(ast);
    return true;
}

bool test_parser_query_shape() {
    char msg[256];
    GqlNode *ast =
        parse_ok("MATCH (a)-[r:KNOWS]->(b) WHERE EXISTS (MATCH (b)-[:LIKES]->(a)) RETURN a, b", msg,
                 sizeof(msg));
    if (!ast) {
        fprintf(stderr, "  %s\n", msg);
        return false;
    }
    if (!has_kind(ast, GQL_MATCH)) {
        fprintf(stderr, "  missing match\n");
        gql_node_free(ast);
        return false;
    }
    if (!has_kind(ast, GQL_RETURN)) {
        fprintf(stderr, "  missing return\n");
        gql_node_free(ast);
        return false;
    }
    if (!has_kind(ast, GQL_PREDICATE)) {
        fprintf(stderr, "  missing predicate\n");
        gql_node_free(ast);
        return false;
    }
    if (!has_kind(ast, GQL_EDGE_PATTERN)) {
        fprintf(stderr, "  missing edge pattern\n");
        gql_node_free(ast);
        return false;
    }
    gql_node_free(ast);

    ast = parse_ok("INSERT (:Person { firstname: 'F', joined: DATE '2023-01-01' })", msg,
                   sizeof(msg));
    if (!ast) {
        fprintf(stderr, "  %s\n", msg);
        return false;
    }
    if (!has_kind(ast, GQL_INSERT)) {
        fprintf(stderr, "  missing insert\n");
        gql_node_free(ast);
        return false;
    }
    const GqlNode *lit = find_kind(ast, GQL_LITERAL);
    if (!lit) {
        fprintf(stderr, "  missing literal\n");
        gql_node_free(ast);
        return false;
    }
    gql_node_free(ast);

    ast = parse_ok("SESSION SET VALUE IF NOT EXISTS $exampleProperty = DATE '2022-10-10'", msg,
                   sizeof(msg));
    if (!ast) {
        fprintf(stderr, "  %s\n", msg);
        return false;
    }
    if (!has_kind(ast, GQL_SESSION_SET)) {
        fprintf(stderr, "  missing session_set\n");
        gql_node_free(ast);
        return false;
    }
    const GqlNode *param = find_kind(ast, GQL_PARAM);
    if (!param || !param->text || strcmp(param->text, "exampleProperty") != 0) {
        fprintf(stderr, "  session set parameter name mismatch\n");
        gql_node_free(ast);
        return false;
    }
    gql_node_free(ast);
    return true;
}

bool test_parser_errors() {
    static const char *bad[] = {
        "MATCH", "CREATE GRAPH", "RETURN", "@#$", "INSERT (a)-",
    };
    const size_t n = sizeof(bad) / sizeof(bad[0]);
    size_t rejected = 0;

    for (size_t i = 0; i < n; i++) {
        GqlParseError err;
        GqlNode *ast = gql_parse_program(bad[i], &err);
        if (ast) {
            fprintf(stderr, "  input unexpectedly parsed: \"%s\"\n", bad[i]);
            gql_node_free(ast);
            return false;
        }
        if (err.code == GQL_ERR_NONE) {
            fprintf(stderr, "  input \"%s\" failed without an error code\n", bad[i]);
            return false;
        }
        rejected++;
    }
    return rejected == n;
}
