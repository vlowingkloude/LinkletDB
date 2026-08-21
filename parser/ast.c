#include "gql_ast.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

enum {
    GQL_AST_INITIAL_CHILD_CAPACITY = 4,
    GQL_AST_CAPACITY_GROWTH_FACTOR = 2,
    GQL_AST_DEFAULT_SUBKIND = 0,
};

static char *duplicate_string(const char *string) {
    if (!string) {
        return NULL;
    }
    const size_t length = strlen(string);
    char *copy = malloc(length + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, string, length + 1);
    return copy;
}

GqlNode *gql_node_new(GqlNodeKind kind) {
    GqlNode *node = calloc(1, sizeof(*node));
    if (!node) {
        return NULL;
    }
    node->kind = kind;
    node->subkind = GQL_AST_DEFAULT_SUBKIND;
    return node;
}

void gql_node_add_child(GqlNode *node, GqlNode *child) {
    if (!node || !child) {
        return;
    }
    if (node->child_count == node->child_capacity) {
        size_t capacity = node->child_capacity
                              ? node->child_capacity * GQL_AST_CAPACITY_GROWTH_FACTOR
                              : GQL_AST_INITIAL_CHILD_CAPACITY;
        GqlNode **grown = realloc(node->children, capacity * sizeof(*grown));
        if (!grown) {
            return;
        }
        node->children = grown;
        node->child_capacity = capacity;
    }
    node->children[node->child_count++] = child;
}

void gql_node_set_text(GqlNode *node, const char *text) {
    if (!node) {
        return;
    }
    free(node->text);
    node->text = duplicate_string(text);
}

void gql_node_take_text(GqlNode *node, char *text) {
    if (!node) {
        free(text);
        return;
    }
    free(node->text);
    node->text = text;
}

GqlNode *gql_node_child(const GqlNode *node, size_t index) {
    if (!node || index >= node->child_count) {
        return NULL;
    }
    return node->children[index];
}

void gql_node_free(GqlNode *node) {
    if (!node) {
        return;
    }
    for (size_t i = 0; i < node->child_count; i++) {
        gql_node_free(node->children[i]);
    }
    free(node->children);
    free(node->text);
    free(node);
}

const char *gql_node_kind_name(GqlNodeKind kind) {
    switch (kind) {
    case GQL_NODE_INVALID:
        return "invalid";
    case GQL_PROGRAM:
        return "program";
    case GQL_SESSION_SET:
        return "session_set";
    case GQL_SESSION_RESET:
        return "session_reset";
    case GQL_SESSION_CLOSE:
        return "session_close";
    case GQL_START_TRANSACTION:
        return "start_transaction";
    case GQL_COMMIT:
        return "commit";
    case GQL_ROLLBACK:
        return "rollback";
    case GQL_PROCEDURE_BODY:
        return "procedure_body";
    case GQL_STATEMENT:
        return "statement";
    case GQL_NEXT_STATEMENT:
        return "next_statement";
    case GQL_CREATE_SCHEMA:
        return "create_schema";
    case GQL_DROP_SCHEMA:
        return "drop_schema";
    case GQL_CREATE_GRAPH:
        return "create_graph";
    case GQL_DROP_GRAPH:
        return "drop_graph";
    case GQL_CREATE_GRAPH_TYPE:
        return "create_graph_type";
    case GQL_DROP_GRAPH_TYPE:
        return "drop_graph_type";
    case GQL_CALL_PROCEDURE:
        return "call_procedure";
    case GQL_NAME:
        return "name";
    case GQL_SCHEMA_REFERENCE:
        return "schema_reference";
    case GQL_CATALOG_PARENT_REFERENCE:
        return "catalog_parent_reference";
    case GQL_GRAPH_REFERENCE:
        return "graph_reference";
    case GQL_GRAPH_TYPE_REFERENCE:
        return "graph_type_reference";
    case GQL_BINDING_TABLE_REFERENCE:
        return "binding_table_reference";
    case GQL_PROCEDURE_REFERENCE:
        return "procedure_reference";
    case GQL_HOME_GRAPH:
        return "home_graph";
    case GQL_CURRENT_GRAPH:
        return "current_graph";
    case GQL_OPEN_GRAPH_TYPE:
        return "open_graph_type";
    case GQL_GRAPH_TYPE_LIKE:
        return "graph_type_like";
    case GQL_NESTED_GRAPH_TYPE:
        return "nested_graph_type";
    case GQL_GRAPH_SOURCE:
        return "graph_source";
    case GQL_ELEMENT_TYPE_LIST:
        return "element_type_list";
    case GQL_NODE_TYPE:
        return "node_type";
    case GQL_EDGE_TYPE:
        return "edge_type";
    case GQL_PROPERTY_TYPE_LIST:
        return "property_type_list";
    case GQL_PROPERTY_TYPE:
        return "property_type";
    case GQL_INSERT:
        return "insert";
    case GQL_SET_STATEMENT:
        return "set_statement";
    case GQL_SET_ITEM:
        return "set_item";
    case GQL_REMOVE_STATEMENT:
        return "remove_statement";
    case GQL_REMOVE_ITEM:
        return "remove_item";
    case GQL_DELETE_STATEMENT:
        return "delete_statement";
    case GQL_COMPOSITE_QUERY:
        return "composite_query";
    case GQL_SET_OPERATION:
        return "set_operation";
    case GQL_MATCH:
        return "match";
    case GQL_OPTIONAL_MATCH:
        return "optional_match";
    case GQL_FILTER:
        return "filter";
    case GQL_LET:
        return "let";
    case GQL_FOR:
        return "for";
    case GQL_ORDER_BY_PAGE:
        return "order_by_page";
    case GQL_RETURN:
        return "return";
    case GQL_FINISH:
        return "finish";
    case GQL_SELECT:
        return "select";
    case GQL_USE_GRAPH:
        return "use_graph";
    case GQL_GRAPH_PATTERN:
        return "graph_pattern";
    case GQL_PATH_PATTERN_LIST:
        return "path_pattern_list";
    case GQL_PATH_PATTERN:
        return "path_pattern";
    case GQL_PATH_EXPRESSION:
        return "path_expression";
    case GQL_NODE_PATTERN:
        return "node_pattern";
    case GQL_EDGE_PATTERN:
        return "edge_pattern";
    case GQL_ELEMENT_FILLER:
        return "element_filler";
    case GQL_LABEL_EXPRESSION:
        return "label_expression";
    case GQL_QUANTIFIER:
        return "quantifier";
    case GQL_INSERT_GRAPH_PATTERN:
        return "insert_graph_pattern";
    case GQL_INSERT_PATH_PATTERN:
        return "insert_path_pattern";
    case GQL_INSERT_NODE_PATTERN:
        return "insert_node_pattern";
    case GQL_INSERT_EDGE_PATTERN:
        return "insert_edge_pattern";
    case GQL_WHERE:
        return "where";
    case GQL_YIELD:
        return "yield";
    case GQL_GROUP_BY:
        return "group_by";
    case GQL_ORDER_BY:
        return "order_by";
    case GQL_SORT_SPEC:
        return "sort_spec";
    case GQL_LIMIT:
        return "limit";
    case GQL_OFFSET:
        return "offset";
    case GQL_HAVING:
        return "having";
    case GQL_AT_SCHEMA:
        return "at_schema";
    case GQL_KEEP:
        return "keep";
    case GQL_VALUE_EXPR:
        return "value_expr";
    case GQL_PREDICATE:
        return "predicate";
    case GQL_LITERAL:
        return "literal";
    case GQL_PARAM:
        return "param";
    case GQL_BINDING_VAR:
        return "binding_var";
    case GQL_PROPERTY_REFERENCE:
        return "property_reference";
    case GQL_FUNCTION_CALL:
        return "function_call";
    case GQL_AGGREGATE:
        return "aggregate";
    case GQL_CASE:
        return "case";
    case GQL_CAST:
        return "cast";
    case GQL_LIST_LITERAL:
        return "list_literal";
    case GQL_RECORD_LITERAL:
        return "record_literal";
    case GQL_FIELD:
        return "field";
    case GQL_PATH_CONSTRUCTOR:
        return "path_constructor";
    case GQL_GRAPH_EXPR:
        return "graph_expr";
    case GQL_BINDING_TABLE_EXPR:
        return "binding_table_expr";
    case GQL_TYPE:
        return "type";
    case GQL_TYPED:
        return "typed";
    case GQL_PROPERTY_SPEC:
        return "property_spec";
    case GQL_VARIABLE_DECL:
        return "variable_decl";
    case GQL_NODE_KIND_COUNT:
        return "node_kind_count";
    }
    return "unknown";
}

static const char *value_operation_name(GqlValueOperation operation) {
    switch (operation) {
    case GQL_OP_OR:
        return "OR";
    case GQL_OP_XOR:
        return "XOR";
    case GQL_OP_AND:
        return "AND";
    case GQL_OP_NOT:
        return "NOT";
    case GQL_OP_EQ:
        return "=";
    case GQL_OP_NE:
        return "<>";
    case GQL_OP_LT:
        return "<";
    case GQL_OP_GT:
        return ">";
    case GQL_OP_LE:
        return "<=";
    case GQL_OP_GE:
        return ">=";
    case GQL_OP_CONCAT:
        return "||";
    case GQL_OP_ADD:
        return "+";
    case GQL_OP_SUB:
        return "-";
    case GQL_OP_MUL:
        return "*";
    case GQL_OP_DIV:
        return "/";
    case GQL_OP_NEG:
        return "-";
    case GQL_OP_POS:
        return "+";
    case GQL_OP_IS_TRUE:
        return "IS TRUE";
    case GQL_OP_IS_NOT_TRUE:
        return "IS NOT TRUE";
    case GQL_OP_IS_FALSE:
        return "IS FALSE";
    case GQL_OP_IS_NOT_FALSE:
        return "IS NOT FALSE";
    case GQL_OP_IS_UNKNOWN:
        return "IS UNKNOWN";
    case GQL_OP_IS_NOT_UNKNOWN:
        return "IS NOT UNKNOWN";
    case GQL_OP_IS_NORMALIZED:
        return "IS NORMALIZED";
    case GQL_OP_IS_NOT_NORMALIZED:
        return "IS NOT NORMALIZED";
    default:
        return "?";
    }
}

static const char *direction_name(GqlDirection d) {
    switch (d) {
    case GQL_DIR_RIGHT:
        return "right";
    case GQL_DIR_LEFT:
        return "left";
    case GQL_DIR_UNDIRECTED:
        return "undirected";
    case GQL_DIR_LEFT_OR_UNDIRECTED:
        return "left-or-undirected";
    case GQL_DIR_UNDIRECTED_OR_RIGHT:
        return "undirected-or-right";
    case GQL_DIR_LEFT_OR_RIGHT:
        return "left-or-right";
    case GQL_DIR_ANY:
        return "any";
    default:
        return "none";
    }
}

static void dump_node(const GqlNode *node, int depth, FILE *out) {
    for (int i = 0; i < depth; i++) {
        fputs("  ", out);
    }
    fputs(gql_node_kind_name(node->kind), out);

    int printed = 0;
    switch (node->kind) {
    case GQL_VALUE_EXPR:
        fprintf(out, " [operation=%s]", value_operation_name((GqlValueOperation)node->subkind));
        printed = 1;
        break;
    case GQL_EDGE_PATTERN:
        fprintf(out, " [dir=%s]", direction_name((GqlDirection)node->subkind));
        printed = 1;
        break;
    case GQL_QUANTIFIER:
        printed = 1;
        break;
    case GQL_LITERAL:
        fprintf(out, " [lit=%d]", node->subkind);
        printed = 1;
        break;
    case GQL_SORT_SPEC:
        fprintf(out, " [order=%d]", node->subkind);
        printed = 1;
        break;
    case GQL_TYPE:
        fprintf(out, " [cat=%d]", node->subkind);
        printed = 1;
        break;
    default:
        if (node->subkind != GQL_AST_DEFAULT_SUBKIND) {
            fprintf(out, " [sub=%d]", node->subkind);
            printed = 1;
        }
        break;
    }

    if (node->text) {
        fprintf(out, " \"%s\"", node->text);
        printed = 1;
    } else if (node->kind == GQL_LITERAL && node->subkind == GQL_LIT_INTEGER) {
        fprintf(out, " %" PRIu64, node->unsigned_integer_value);
        printed = 1;
    } else if (node->kind == GQL_LITERAL && node->subkind == GQL_LIT_FLOAT) {
        fprintf(out, " %g", node->floating_value);
        printed = 1;
    }
    (void)printed;
    fputc('\n', out);

    for (size_t i = 0; i < node->child_count; i++) {
        dump_node(node->children[i], depth + 1, out);
    }
}

void gql_ast_dump(const GqlNode *root, FILE *out) {
    if (!out) {
        out = stderr;
    }
    if (!root) {
        fputs("(null)\n", out);
        return;
    }
    dump_node(root, 0, out);
}
