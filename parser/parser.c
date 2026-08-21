#include "gql_parser.h"
#include "gql_lexer.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct GqlParser {
    GqlLexer lexer;
    GqlToken previous_token;
    GqlParseError error;
    int failed;
} GqlParser;

enum {
    GQL_SESSION_SET_SCHEMA = 1,
    GQL_SESSION_SET_GRAPH = 2,
    GQL_SESSION_SET_TIMEZONE = 3,
    GQL_SESSION_SET_PARAMETER = 4,

    GQL_PARAMETER_GRAPH = 1,
    GQL_PARAMETER_BINDING_TABLE = 2,
    GQL_PARAMETER_VALUE = 3,

    GQL_TYPED_DOUBLE_COLON = 1,
    GQL_TYPED_KEYWORD = 2,

    GQL_PARAMETER_SINGLE = 1,
    GQL_PARAMETER_DOUBLE = 2,
};

static void gql_parser_init(GqlParser *parser, const char *source) {
    *parser = (GqlParser){0};
    gql_lexer_init(&parser->lexer, source);
    parser->failed = 0;
}

static void parse_error(GqlParser *parser, int code, const char *fmt, ...) {
    if (parser->failed) {
        return;
    }
    parser->failed = 1;
    parser->error.code = code;
    const GqlToken *t = gql_lexer_peek(&parser->lexer, 0);
    parser->error.line = t->line;
    parser->error.column = t->column;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(parser->error.message, sizeof(parser->error.message), fmt, ap);
    va_end(ap);
}

static void unexpected(GqlParser *parser, const char *what) {
    const GqlToken *t = gql_lexer_peek(&parser->lexer, 0);
    parse_error(parser, GQL_ERR_UNEXPECTED_TOKEN, "expected %s but found %s", what,
                gql_token_kind_name(t->kind));
}

static const GqlToken *current_token(GqlParser *parser) {
    return gql_lexer_peek(&parser->lexer, 0);
}
static const GqlToken *peek(GqlParser *parser, int k) {
    return gql_lexer_peek(&parser->lexer, k);
}

static int at(GqlParser *parser, GqlTokenKind k) {
    return current_token(parser)->kind == k;
}

static void advance(GqlParser *parser) {
    parser->previous_token = *gql_lexer_peek(&parser->lexer, 0);
    gql_lexer_advance(&parser->lexer);
}

static int match(GqlParser *parser, GqlTokenKind k) {
    if (at(parser, k)) {
        advance(parser);
        return 1;
    }
    return 0;
}

static int expect(GqlParser *parser, GqlTokenKind k, const char *what) {
    if (match(parser, k)) {
        return 1;
    }
    unexpected(parser, what);
    return 0;
}

static int token_is_name(const GqlToken *t) {
    if (t->kind == TOK_IDENT || t->kind == TOK_DELIM_IDENT) {
        return 1;
    }
    if (t->kind == TOK_STR_LIT && t->quote == GQL_QUOTE_DOUBLE) {
        return 1;
    }
    return gql_token_nonreserved(t->kind);
}

static GqlNode *name_node(const GqlToken *t) {
    GqlNode *n = gql_node_new(GQL_NAME);
    char *s = gql_token_decoded_text(t);
    gql_node_take_text(n, s);
    return n;
}

static GqlNode *parse_name(GqlParser *parser) {
    GqlToken t = *current_token(parser);
    if (!token_is_name(&t)) {
        unexpected(parser, "an identifier");
        return NULL;
    }
    advance(parser);
    return name_node(&t);
}

static GqlNode *parse_program(GqlParser *parser);
static void parse_program_activity(GqlParser *parser, GqlNode *prog);
static GqlNode *parse_procedure_body(GqlParser *parser);
static GqlNode *parse_statement(GqlParser *parser);
static GqlNode *parse_value_expression(GqlParser *parser);
static GqlNode *parse_search_condition(GqlParser *parser);
static GqlNode *parse_graph_pattern(GqlParser *parser);
static GqlNode *parse_graph_expression(GqlParser *parser);
static GqlNode *parse_value_type(GqlParser *parser);
static GqlNode *parse_schema_reference(GqlParser *parser);
static GqlNode *parse_catalog_parent_reference(GqlParser *parser);
static GqlNode *parse_typed_optional(GqlParser *parser);

static GqlNode *parse_session_set(GqlParser *parser);
static GqlNode *parse_session_reset(GqlParser *parser);
static GqlNode *parse_session_set_parameter_name(GqlParser *parser);
static GqlNode *parse_session_reset_arguments(GqlParser *parser);
static GqlNode *parse_start_transaction(GqlParser *parser);
static GqlNode *parse_end_transaction(GqlParser *parser);

static GqlNode *parse_simple_catalog_modifying_statement(GqlParser *parser);
static GqlNode *parse_create_schema(GqlParser *parser);
static GqlNode *parse_drop_schema(GqlParser *parser);
static GqlNode *parse_create_graph(GqlParser *parser);
static GqlNode *parse_create_graph_type(GqlParser *parser);
static GqlNode *parse_drop_graph(GqlParser *parser);
static GqlNode *parse_drop_graph_type(GqlParser *parser);
static GqlNode *parse_graph_type_source(GqlParser *parser);
static GqlNode *parse_graph_type_source_statement(GqlParser *parser);
static GqlNode *parse_nested_graph_type(GqlParser *parser);

static GqlNode *parse_catalog_schema_parent_and_name(GqlParser *parser);
static GqlNode *parse_catalog_graph_parent_and_name(GqlParser *parser);
static GqlNode *parse_catalog_graph_type_parent_and_name(GqlParser *parser);
static GqlNode *parse_graph_reference(GqlParser *parser);
static GqlNode *parse_binding_table_expression(GqlParser *parser);

static GqlNode *parse_insert_statement(GqlParser *parser);
static GqlNode *parse_set_statement(GqlParser *parser);
static GqlNode *parse_set_item(GqlParser *parser);
static GqlNode *parse_remove_statement(GqlParser *parser);
static GqlNode *parse_remove_item(GqlParser *parser);
static GqlNode *parse_delete_statement(GqlParser *parser);

static GqlNode *parse_match_statement(GqlParser *parser);
static GqlNode *parse_graph_pattern_binding_table(GqlParser *parser);
static GqlNode *parse_filter_statement(GqlParser *parser);
static GqlNode *parse_let_statement(GqlParser *parser);
static GqlNode *parse_for_statement(GqlParser *parser);
static GqlNode *parse_order_by_and_page(GqlParser *parser);
static GqlNode *parse_return_statement(GqlParser *parser);
static GqlNode *parse_select_statement(GqlParser *parser);
static GqlNode *parse_call_statement(GqlParser *parser);

static GqlNode *parse_path_pattern(GqlParser *parser);
static GqlNode *parse_path_pattern_expression(GqlParser *parser);
static GqlNode *parse_path_term(GqlParser *parser);
static GqlNode *parse_path_factor(GqlParser *parser);
static GqlNode *parse_path_primary(GqlParser *parser);
static GqlNode *parse_node_pattern(GqlParser *parser);
static GqlNode *parse_edge_pattern(GqlParser *parser);
static GqlNode *parse_element_filler(GqlParser *parser, int insert);
static GqlNode *parse_parenthesized_path(GqlParser *parser);
static GqlNode *parse_simplified_path(GqlParser *parser);
static GqlNode *parse_label_expression(GqlParser *parser);
static GqlNode *parse_label_unary(GqlParser *parser);
static GqlNode *parse_label_set_specification(GqlParser *parser);
static GqlNode *parse_graph_pattern_quantifier(GqlParser *parser);

static GqlNode *parse_insert_graph_pattern(GqlParser *parser);
static GqlNode *parse_insert_path_pattern(GqlParser *parser);
static GqlNode *parse_insert_node_pattern(GqlParser *parser);
static GqlNode *parse_insert_edge_pattern(GqlParser *parser);

static GqlNode *parse_element_type_specification(GqlParser *parser);
static GqlNode *parse_node_type_specification(GqlParser *parser);
static GqlNode *parse_edge_type_specification(GqlParser *parser);
static GqlNode *parse_node_type_endpoint(GqlParser *parser);
static GqlNode *parse_edge_type_filler(GqlParser *parser);
static GqlNode *parse_label_set_phrase(GqlParser *parser);
static GqlNode *parse_property_types_specification(GqlParser *parser);

static void parse_not_null(GqlParser *parser, GqlNode *node);
static GqlNode *parse_yield_clause(GqlParser *parser);
static GqlNode *parse_group_by_clause(GqlParser *parser);
static GqlNode *parse_order_by_clause(GqlParser *parser);
static GqlNode *parse_limit_clause(GqlParser *parser);
static GqlNode *parse_offset_clause(GqlParser *parser);
static GqlNode *parse_use_graph_clause(GqlParser *parser);
static GqlNode *parse_non_negative_integer(GqlParser *parser);

static GqlNode *parse_binary(GqlParser *parser, int min_prec);
static GqlNode *parse_unary(GqlParser *parser);
static GqlNode *parse_postfix(GqlParser *parser);
static GqlNode *parse_is_predicate(GqlParser *parser, GqlNode *operand);
static GqlNode *parse_exists_predicate(GqlParser *parser);
static GqlNode *parse_all_different_predicate(GqlParser *parser);
static GqlNode *parse_same_predicate(GqlParser *parser);
static GqlNode *parse_property_exists_predicate(GqlParser *parser);
static GqlNode *parse_binding_variable_reference(GqlParser *parser);
static GqlNode *parse_parameter(GqlParser *parser);
static GqlNode *parse_property_key_value_pair(GqlParser *parser);
static GqlNode *parse_primary(GqlParser *parser);
static GqlNode *parse_value_expression_primary(GqlParser *parser);
static GqlNode *parse_number_literal(GqlParser *parser);
static GqlNode *parse_list_literal(GqlParser *parser, const char *type_name);
static GqlNode *parse_record_literal(GqlParser *parser);
static GqlNode *parse_case_expression(GqlParser *parser);
static GqlNode *parse_cast_specification(GqlParser *parser);
static GqlNode *parse_function_or_aggregate(GqlParser *parser);
static GqlNode *parse_aggregate(GqlParser *parser, GqlAggregateKind agg);
static GqlNode *parse_function(GqlParser *parser, GqlFunctionKind fn);

static int edge_pattern_start(GqlTokenKind k);
static int edge_direction(GqlTokenKind open, GqlTokenKind close);
static int is_edge_close(GqlTokenKind k);
static int binary_operator_precedence(GqlTokenKind k, GqlValueOperation *operation);
static int is_predefined_type_token(GqlTokenKind k);
static int lookup_aggregate(const char *name, GqlAggregateKind *out);
static int lookup_function(const char *name, GqlFunctionKind *out);
static int ieq(const char *a, const char *b);
static int ascii_ieq(const GqlToken *t, const char *s);
static char *duplicate_string(const char *s);
static char *slice(GqlToken first, GqlToken last);

static GqlNode *parse_program(GqlParser *parser) {
    GqlNode *prog = gql_node_new(GQL_PROGRAM);
    if (at(parser, TOK_SESSION) && peek(parser, 1)->kind == TOK_CLOSE) {
        advance(parser);
        advance(parser);
        gql_node_add_child(prog, gql_node_new(GQL_SESSION_CLOSE));
        return prog;
    }
    parse_program_activity(parser, prog);
    if (!parser->failed && at(parser, TOK_SESSION) && peek(parser, 1)->kind == TOK_CLOSE) {
        advance(parser);
        advance(parser);
        gql_node_add_child(prog, gql_node_new(GQL_SESSION_CLOSE));
    }
    return prog;
}

static void parse_program_activity(GqlParser *parser, GqlNode *prog) {
    if (at(parser, TOK_SESSION)) {
        if (peek(parser, 1)->kind == TOK_SET) {

            while (!parser->failed && at(parser, TOK_SESSION) && peek(parser, 1)->kind == TOK_SET) {
                GqlNode *set = parse_session_set(parser);
                if (set) {
                    gql_node_add_child(prog, set);
                }
            }
            while (!parser->failed && at(parser, TOK_SESSION) &&
                   peek(parser, 1)->kind == TOK_RESET) {
                GqlNode *reset = parse_session_reset(parser);
                if (reset) {
                    gql_node_add_child(prog, reset);
                }
            }
            return;
        }
        if (peek(parser, 1)->kind == TOK_RESET) {
            while (!parser->failed && at(parser, TOK_SESSION) &&
                   peek(parser, 1)->kind == TOK_RESET) {
                GqlNode *reset = parse_session_reset(parser);
                if (reset) {
                    gql_node_add_child(prog, reset);
                }
            }
            return;
        }
        unexpected(parser, "SESSION SET, SESSION RESET or SESSION CLOSE");
        return;
    }

    if (at(parser, TOK_START)) {
        gql_node_add_child(prog, parse_start_transaction(parser));
        return;
    }
    if (at(parser, TOK_COMMIT) || at(parser, TOK_ROLLBACK)) {
        gql_node_add_child(prog, parse_end_transaction(parser));
        return;
    }

    GqlNode *proc = parse_procedure_body(parser);
    if (proc) {
        gql_node_add_child(prog, proc);
    }
    if (!parser->failed && (at(parser, TOK_COMMIT) || at(parser, TOK_ROLLBACK))) {
        gql_node_add_child(prog, parse_end_transaction(parser));
    }
}

static GqlNode *parse_session_set(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_SESSION_SET);
    expect(parser, TOK_SESSION, "SESSION");
    expect(parser, TOK_SET, "SET");

    if (at(parser, TOK_SCHEMA)) {
        advance(parser);
        GqlNode *ref = parse_schema_reference(parser);
        if (ref) {
            gql_node_add_child(node, ref);
        }
        node->subkind = GQL_SESSION_SET_SCHEMA;
        return node;
    }
    if (at(parser, TOK_TIME)) {
        advance(parser);
        expect(parser, TOK_ZONE, "ZONE");

        GqlNode *tz = parse_value_expression(parser);
        if (tz) {
            gql_node_add_child(node, tz);
        }
        node->subkind = GQL_SESSION_SET_TIMEZONE;
        return node;
    }
    if (at(parser, TOK_VALUE)) {
        advance(parser);
        GqlNode *param = parse_session_set_parameter_name(parser);
        if (param) {
            gql_node_add_child(node, param);
        }

        if (match(parser, TOK_EQUALS)) {
            GqlNode *v = parse_value_expression(parser);
            if (v) {
                gql_node_add_child(node, v);
            }
        }
        node->subkind = GQL_SESSION_SET_PARAMETER;
        node->integer_value = GQL_PARAMETER_VALUE;
        return node;
    }

    int had_property = match(parser, TOK_PROPERTY);

    if (at(parser, TOK_GRAPH)) {
        advance(parser);
        GqlNode *expr;
        if (at(parser, TOK_IF) || at(parser, TOK_PARAM)) {

            GqlNode *param = parse_session_set_parameter_name(parser);
            if (param) {
                gql_node_add_child(node, param);
            }

            if (match(parser, TOK_EQUALS)) {
                expr = parse_graph_expression(parser);
                if (expr) {
                    gql_node_add_child(node, expr);
                }
            }
            node->subkind = GQL_SESSION_SET_PARAMETER;
            node->integer_value = GQL_PARAMETER_GRAPH;
            return node;
        }
        expr = parse_graph_expression(parser);
        if (expr) {
            gql_node_add_child(node, expr);
        }
        node->subkind = GQL_SESSION_SET_GRAPH;
        return node;
    }

    if (at(parser, TOK_BINDING) || at(parser, TOK_TABLE)) {
        match(parser, TOK_BINDING);
        expect(parser, TOK_TABLE, "TABLE");
        GqlNode *param = parse_session_set_parameter_name(parser);
        if (param) {
            gql_node_add_child(node, param);
        }
        if (match(parser, TOK_EQUALS)) {
            GqlNode *expr = parse_binding_table_expression(parser);
            if (expr) {
                gql_node_add_child(node, expr);
            }
        }
        node->subkind = GQL_SESSION_SET_PARAMETER;
        node->integer_value = GQL_PARAMETER_BINDING_TABLE;
        return node;
    }

    (void)had_property;
    unexpected(parser, "SCHEMA, TIME ZONE, VALUE, GRAPH or TABLE");
    gql_node_free(node);
    return NULL;
}

static GqlNode *parse_session_set_parameter_name(GqlParser *parser) {
    if (at(parser, TOK_IF)) {
        advance(parser);
        expect(parser, TOK_NOT, "NOT");
        expect(parser, TOK_EXISTS, "EXISTS");
    }
    return parse_parameter(parser);
}

static GqlNode *parse_session_reset(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_SESSION_RESET);
    expect(parser, TOK_SESSION, "SESSION");
    expect(parser, TOK_RESET, "RESET");

    if (at(parser, TOK_ALL) || at(parser, TOK_PARAMETERS) || at(parser, TOK_CHARACTERISTICS) ||
        at(parser, TOK_SCHEMA) || at(parser, TOK_PROPERTY) || at(parser, TOK_GRAPH) ||
        at(parser, TOK_TIME) || at(parser, TOK_PARAMETER) || at(parser, TOK_PARAM)) {
        GqlNode *arg = parse_session_reset_arguments(parser);
        if (arg) {
            gql_node_add_child(node, arg);
        }
    }
    return node;
}

static GqlNode *parse_session_reset_arguments(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_NAME);
    match(parser, TOK_ALL);
    if (match(parser, TOK_PARAMETERS) || match(parser, TOK_CHARACTERISTICS)) {
        gql_node_set_text(node, "characteristics");
    } else if (match(parser, TOK_SCHEMA)) {
        gql_node_set_text(node, "schema");
    } else if (at(parser, TOK_PROPERTY) || at(parser, TOK_GRAPH)) {
        match(parser, TOK_PROPERTY);
        expect(parser, TOK_GRAPH, "GRAPH");
        gql_node_set_text(node, "graph");
    } else if (match(parser, TOK_TIME)) {
        expect(parser, TOK_ZONE, "ZONE");
        gql_node_set_text(node, "time zone");
    } else {
        match(parser, TOK_PARAMETER);
        GqlNode *param = parse_parameter(parser);
        if (param) {
            gql_node_add_child(node, param);
        }
        gql_node_set_text(node, "parameter");
    }
    return node;
}

static GqlNode *parse_start_transaction(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_START_TRANSACTION);
    expect(parser, TOK_START, "START");
    expect(parser, TOK_TRANSACTION, "TRANSACTION");

    while (!parser->failed) {
        GqlNode *mode = gql_node_new(GQL_NAME);
        if (match(parser, TOK_READ)) {
            if (match(parser, TOK_ONLY)) {
                gql_node_set_text(mode, "READ ONLY");
            } else if (match(parser, TOK_WRITE)) {
                gql_node_set_text(mode, "READ WRITE");
            } else {
                unexpected(parser, "ONLY or WRITE");
                gql_node_free(mode);
                return node;
            }
        } else {
            gql_node_free(mode);
            break;
        }
        gql_node_add_child(node, mode);
        if (!match(parser, TOK_COMMA)) {
            break;
        }
    }
    return node;
}

static GqlNode *parse_end_transaction(GqlParser *parser) {
    if (match(parser, TOK_COMMIT)) {
        return gql_node_new(GQL_COMMIT);
    }
    if (match(parser, TOK_ROLLBACK)) {
        return gql_node_new(GQL_ROLLBACK);
    }
    unexpected(parser, "COMMIT or ROLLBACK");
    return NULL;
}

static GqlNode *parse_procedure_body(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_PROCEDURE_BODY);

    if (at(parser, TOK_AT)) {
        advance(parser);
        GqlNode *at = gql_node_new(GQL_AT_SCHEMA);
        GqlNode *ref = parse_schema_reference(parser);
        if (ref) {
            gql_node_add_child(at, ref);
        }
        gql_node_add_child(node, at);
    }

    if (at(parser, TOK_PROPERTY) || at(parser, TOK_GRAPH) || at(parser, TOK_BINDING) ||
        at(parser, TOK_TABLE) || at(parser, TOK_VALUE)) {

        while (!parser->failed &&
               (at(parser, TOK_PROPERTY) || at(parser, TOK_GRAPH) || at(parser, TOK_BINDING) ||
                at(parser, TOK_TABLE) || at(parser, TOK_VALUE))) {
            GqlNode *def = gql_node_new(GQL_VARIABLE_DECL);
            match(parser, TOK_PROPERTY);
            if (at(parser, TOK_GRAPH) || at(parser, TOK_BINDING) || at(parser, TOK_TABLE) ||
                at(parser, TOK_VALUE)) {
                advance(parser);
                GqlNode *name = parse_name(parser);
                if (name) {
                    gql_node_add_child(def, name);
                }
                if (match(parser, TOK_EQUALS)) {
                    GqlNode *e = parse_value_expression(parser);
                    if (e) {
                        gql_node_add_child(def, e);
                    }
                }
                gql_node_add_child(node, def);
            } else {
                gql_node_free(def);
                break;
            }
        }
    }

    GqlNode *stmt = parse_statement(parser);
    if (stmt) {
        gql_node_add_child(node, stmt);
    }
    while (!parser->failed && at(parser, TOK_NEXT)) {
        advance(parser);
        GqlNode *next = gql_node_new(GQL_NEXT_STATEMENT);

        if (at(parser, TOK_YIELD)) {
            GqlNode *y = parse_yield_clause(parser);
            if (y) {
                gql_node_add_child(next, y);
            }
        }
        GqlNode *s2 = parse_statement(parser);
        if (s2) {
            gql_node_add_child(next, s2);
        }
        gql_node_add_child(node, next);
    }
    return node;
}

static GqlNode *parse_statement(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_STATEMENT);

    if (at(parser, TOK_CREATE) || at(parser, TOK_DROP)) {

        GqlNode *stmt = parse_simple_catalog_modifying_statement(parser);
        if (stmt) {
            gql_node_add_child(node, stmt);
        }
        return node;
    }

    while (true) {
        if (parser->failed) {
            break;
        }
        GqlNode *clause = NULL;
        if (at(parser, TOK_USE)) {
            clause = parse_use_graph_clause(parser);
        } else if (at(parser, TOK_MATCH) || at(parser, TOK_OPTIONAL)) {
            clause = parse_match_statement(parser);
        } else if (at(parser, TOK_INSERT)) {
            clause = parse_insert_statement(parser);
        } else if (at(parser, TOK_SET)) {
            clause = parse_set_statement(parser);
        } else if (at(parser, TOK_REMOVE)) {
            clause = parse_remove_statement(parser);
        } else if (at(parser, TOK_DELETE) || at(parser, TOK_DETACH) || at(parser, TOK_NODETACH)) {
            clause = parse_delete_statement(parser);
        } else if (at(parser, TOK_FILTER)) {
            clause = parse_filter_statement(parser);
        } else if (at(parser, TOK_LET)) {
            clause = parse_let_statement(parser);
        } else if (at(parser, TOK_FOR)) {
            clause = parse_for_statement(parser);
        } else if (at(parser, TOK_ORDER) || at(parser, TOK_OFFSET) ||
                   at(parser, TOK_SKIP_RESERVED_WORD) || at(parser, TOK_LIMIT)) {
            clause = parse_order_by_and_page(parser);
        } else if (at(parser, TOK_RETURN)) {
            clause = parse_return_statement(parser);
        } else if (at(parser, TOK_FINISH)) {
            advance(parser);
            clause = gql_node_new(GQL_FINISH);
        } else if (at(parser, TOK_SELECT)) {
            clause = parse_select_statement(parser);
        } else if (at(parser, TOK_CALL) || at(parser, TOK_OPTIONAL)) {
            clause = parse_call_statement(parser);
        } else {
            break;
        }
        if (!clause) {
            break;
        }
        gql_node_add_child(node, clause);
    }

    if (node->child_count == 0) {
        unexpected(parser, "a statement");
        gql_node_free(node);
        return NULL;
    }
    return node;
}

static int create_is_graph_type(GqlParser *parser) {
    int i = 1;
    if (peek(parser, i)->kind == TOK_OR) {
        i += 2;
        if (peek(parser, i)->kind == TOK_PROPERTY) {
            i += 1;
        }
        if (peek(parser, i)->kind == TOK_GRAPH) {
            i += 1;
        }
        return peek(parser, i)->kind == TOK_TYPE;
    }
    if (peek(parser, i)->kind == TOK_PROPERTY) {
        i += 1;
    }
    if (peek(parser, i)->kind == TOK_GRAPH) {
        i += 1;
    }
    return peek(parser, i)->kind == TOK_TYPE;
}

static GqlNode *parse_simple_catalog_modifying_statement(GqlParser *parser) {
    if (at(parser, TOK_CREATE)) {
        if (peek(parser, 1)->kind == TOK_SCHEMA) {
            return parse_create_schema(parser);
        }
        if (create_is_graph_type(parser)) {
            return parse_create_graph_type(parser);
        }
        return parse_create_graph(parser);
    }
    if (at(parser, TOK_DROP)) {
        if (peek(parser, 1)->kind == TOK_SCHEMA) {
            return parse_drop_schema(parser);
        }
        if (peek(parser, 1)->kind == TOK_GRAPH && peek(parser, 2)->kind == TOK_TYPE) {
            return parse_drop_graph_type(parser);
        }
        if (peek(parser, 1)->kind == TOK_PROPERTY && peek(parser, 2)->kind == TOK_GRAPH &&
            peek(parser, 3)->kind == TOK_TYPE) {
            return parse_drop_graph_type(parser);
        }
        return parse_drop_graph(parser);
    }
    unexpected(parser, "CREATE or DROP");
    return NULL;
}

static GqlNode *parse_create_schema(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_CREATE_SCHEMA);
    expect(parser, TOK_CREATE, "CREATE");
    expect(parser, TOK_SCHEMA, "SCHEMA");
    if (at(parser, TOK_IF)) {
        advance(parser);
        expect(parser, TOK_NOT, "NOT");
        expect(parser, TOK_EXISTS, "EXISTS");
        node->integer_value = GQL_CATALOG_MODIFICATION_CONDITIONAL;
    }
    GqlNode *name = parse_catalog_schema_parent_and_name(parser);
    if (name) {
        gql_node_add_child(node, name);
    }
    return node;
}

static GqlNode *parse_drop_schema(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_DROP_SCHEMA);
    expect(parser, TOK_DROP, "DROP");
    expect(parser, TOK_SCHEMA, "SCHEMA");
    if (at(parser, TOK_IF)) {
        advance(parser);
        expect(parser, TOK_EXISTS, "EXISTS");
        node->integer_value = GQL_CATALOG_MODIFICATION_CONDITIONAL;
    }
    GqlNode *name = parse_catalog_schema_parent_and_name(parser);
    if (name) {
        gql_node_add_child(node, name);
    }
    return node;
}

static GqlNode *parse_create_graph(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_CREATE_GRAPH);
    expect(parser, TOK_CREATE, "CREATE");

    if (at(parser, TOK_OR)) {
        advance(parser);
        expect(parser, TOK_REPLACE, "REPLACE");
        node->integer_value = GQL_CATALOG_MODIFICATION_OR_REPLACE;
    }
    match(parser, TOK_PROPERTY);
    expect(parser, TOK_GRAPH, "GRAPH");
    if (at(parser, TOK_IF)) {
        advance(parser);
        expect(parser, TOK_NOT, "NOT");
        expect(parser, TOK_EXISTS, "EXISTS");
        if (node->integer_value == GQL_CATALOG_MODIFICATION_NONE) {
            node->integer_value = GQL_CATALOG_MODIFICATION_CONDITIONAL;
        }
    }

    GqlNode *name = parse_catalog_graph_parent_and_name(parser);
    if (name) {
        gql_node_add_child(node, name);
    }

    GqlNode *type = parse_graph_type_source(parser);
    if (type) {
        gql_node_add_child(node, type);
    }

    if (at(parser, TOK_AS)) {
        advance(parser);
        expect(parser, TOK_COPY, "COPY");
        expect(parser, TOK_OF, "OF");
        GqlNode *source = gql_node_new(GQL_GRAPH_SOURCE);
        GqlNode *expr = parse_graph_expression(parser);
        if (expr) {
            gql_node_add_child(source, expr);
        }
        gql_node_add_child(node, source);
    }
    return node;
}

static GqlNode *parse_create_graph_type(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_CREATE_GRAPH_TYPE);
    expect(parser, TOK_CREATE, "CREATE");
    if (at(parser, TOK_OR)) {
        advance(parser);
        expect(parser, TOK_REPLACE, "REPLACE");
        node->integer_value = GQL_CATALOG_MODIFICATION_OR_REPLACE;
    }
    match(parser, TOK_PROPERTY);
    expect(parser, TOK_GRAPH, "GRAPH");
    expect(parser, TOK_TYPE, "TYPE");
    if (at(parser, TOK_IF)) {
        advance(parser);
        expect(parser, TOK_NOT, "NOT");
        expect(parser, TOK_EXISTS, "EXISTS");
        if (node->integer_value == GQL_CATALOG_MODIFICATION_NONE) {
            node->integer_value = GQL_CATALOG_MODIFICATION_CONDITIONAL;
        }
    }
    GqlNode *name = parse_catalog_graph_type_parent_and_name(parser);
    if (name) {
        gql_node_add_child(node, name);
    }

    GqlNode *source = parse_graph_type_source_statement(parser);
    if (source) {
        gql_node_add_child(node, source);
    }
    return node;
}

static GqlNode *parse_drop_graph(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_DROP_GRAPH);
    expect(parser, TOK_DROP, "DROP");
    match(parser, TOK_PROPERTY);
    expect(parser, TOK_GRAPH, "GRAPH");
    if (at(parser, TOK_IF)) {
        advance(parser);
        expect(parser, TOK_EXISTS, "EXISTS");
        node->integer_value = GQL_CATALOG_MODIFICATION_CONDITIONAL;
    }
    GqlNode *name = parse_catalog_graph_parent_and_name(parser);
    if (name) {
        gql_node_add_child(node, name);
    }
    return node;
}

static GqlNode *parse_drop_graph_type(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_DROP_GRAPH_TYPE);
    expect(parser, TOK_DROP, "DROP");
    match(parser, TOK_PROPERTY);
    expect(parser, TOK_GRAPH, "GRAPH");
    expect(parser, TOK_TYPE, "TYPE");
    if (at(parser, TOK_IF)) {
        advance(parser);
        expect(parser, TOK_EXISTS, "EXISTS");
        node->integer_value = GQL_CATALOG_MODIFICATION_CONDITIONAL;
    }
    GqlNode *name = parse_catalog_graph_type_parent_and_name(parser);
    if (name) {
        gql_node_add_child(node, name);
    }
    return node;
}

static GqlNode *parse_graph_type_source_statement(GqlParser *parser) {
    if (at(parser, TOK_AS)) {
        advance(parser);
    }
    if (at(parser, TOK_COPY)) {
        advance(parser);
        expect(parser, TOK_OF, "OF");
        GqlNode *n = gql_node_new(GQL_GRAPH_TYPE_REFERENCE);
        GqlNode *name = parse_name(parser);
        if (name) {
            gql_node_add_child(n, name);
        }
        return n;
    }
    if (at(parser, TOK_LIKE)) {
        advance(parser);
        GqlNode *n = gql_node_new(GQL_GRAPH_TYPE_LIKE);
        GqlNode *e = parse_graph_expression(parser);
        if (e) {
            gql_node_add_child(n, e);
        }
        return n;
    }
    if (at(parser, TOK_LBRACE)) {
        return parse_nested_graph_type(parser);
    }
    unexpected(parser, "a graph type source");
    return NULL;
}

static GqlNode *parse_graph_type_source(GqlParser *parser) {
    if (at(parser, TOK_LIKE)) {
        advance(parser);
        GqlNode *n = gql_node_new(GQL_GRAPH_TYPE_LIKE);
        GqlNode *e = parse_graph_expression(parser);
        if (e) {
            gql_node_add_child(n, e);
        }
        return n;
    }
    GqlNode *typed = parse_typed_optional(parser);
    if (at(parser, TOK_ANY)) {
        advance(parser);
        GqlNode *n = gql_node_new(GQL_OPEN_GRAPH_TYPE);
        match(parser, TOK_PROPERTY);
        match(parser, TOK_GRAPH);
        if (typed) {
            gql_node_add_child(n, typed);
        }
        return n;
    }
    if (at(parser, TOK_LBRACE) || at(parser, TOK_PROPERTY) || at(parser, TOK_GRAPH)) {
        match(parser, TOK_PROPERTY);
        match(parser, TOK_GRAPH);
        GqlNode *n = parse_nested_graph_type(parser);
        if (typed) {
            gql_node_add_child(n, typed);
        }
        return n;
    }
    GqlNode *n = gql_node_new(GQL_GRAPH_TYPE_REFERENCE);
    GqlNode *name = parse_name(parser);
    if (name) {
        gql_node_add_child(n, name);
    }
    if (typed) {
        gql_node_add_child(n, typed);
    }
    return n;
}

static GqlNode *parse_typed_optional(GqlParser *parser) {
    GqlNode *typed = NULL;
    if (match(parser, TOK_DOUBLE_COLON)) {
        typed = gql_node_new(GQL_TYPED);
        typed->subkind = GQL_TYPED_DOUBLE_COLON;
    } else if (match(parser, TOK_TYPED)) {
        typed = gql_node_new(GQL_TYPED);
        typed->subkind = GQL_TYPED_KEYWORD;
    }
    return typed;
}

static GqlNode *parse_catalog_schema_parent_and_name(GqlParser *parser) {
    GqlNode *name = gql_node_new(GQL_NAME);
    GqlNode *parent = NULL;
    if (at(parser, TOK_SOLIDUS)) {
        parent = gql_node_new(GQL_CATALOG_PARENT_REFERENCE);
        GqlToken first = *current_token(parser);
        advance(parser);
        while (!parser->failed && at(parser, TOK_IDENT) && peek(parser, 1)->kind == TOK_SOLIDUS) {
            advance(parser);
            advance(parser);
        }
        char *s = slice(first, parser->previous_token);
        gql_node_take_text(parent, s);
    }
    GqlNode *n = parse_name(parser);
    if (n) {
        gql_node_set_text(name, n->text);
        gql_node_free(n);
    }
    if (parent) {
        gql_node_add_child(name, parent);
    }
    return name;
}

static GqlNode *parse_catalog_graph_parent_and_name(GqlParser *parser) {
    GqlNode *name = gql_node_new(GQL_NAME);
    GqlNode *parent = parse_catalog_parent_reference(parser);
    GqlNode *n = parse_name(parser);
    if (n) {
        gql_node_set_text(name, n->text);
        gql_node_free(n);
    }
    if (parent) {
        gql_node_add_child(name, parent);
    }
    return name;
}

static GqlNode *parse_catalog_graph_type_parent_and_name(GqlParser *parser) {
    GqlNode *name = gql_node_new(GQL_NAME);
    GqlNode *parent = parse_catalog_parent_reference(parser);
    GqlNode *n = parse_name(parser);
    if (n) {
        gql_node_set_text(name, n->text);
        gql_node_free(n);
    }
    if (parent) {
        gql_node_add_child(name, parent);
    }
    return name;
}

static GqlNode *parse_catalog_parent_reference(GqlParser *parser) {
    if (at(parser, TOK_SOLIDUS)) {
        GqlToken first = *current_token(parser);
        advance(parser);

        while (!parser->failed && at(parser, TOK_IDENT) && peek(parser, 1)->kind == TOK_SOLIDUS) {
            advance(parser);
            advance(parser);
        }

        match(parser, TOK_SOLIDUS);
        while (!parser->failed && at(parser, TOK_IDENT) && peek(parser, 1)->kind == TOK_PERIOD) {
            advance(parser);
            advance(parser);
        }
        GqlNode *n = gql_node_new(GQL_CATALOG_PARENT_REFERENCE);
        gql_node_take_text(n, slice(first, parser->previous_token));
        return n;
    }
    if (at(parser, TOK_DOUBLE_PERIOD) || at(parser, TOK_HOME_SCHEMA) ||
        at(parser, TOK_CURRENT_SCHEMA) || at(parser, TOK_PERIOD) || at(parser, TOK_PARAM)) {
        GqlToken first = *current_token(parser);
        if (at(parser, TOK_DOUBLE_PERIOD)) {
            advance(parser);
            while (!parser->failed && at(parser, TOK_SOLIDUS) &&
                   peek(parser, 1)->kind == TOK_DOUBLE_PERIOD) {
                advance(parser);
                advance(parser);
            }
            match(parser, TOK_SOLIDUS);
        } else {
            advance(parser);
        }
        match(parser, TOK_SOLIDUS);
        while (!parser->failed && at(parser, TOK_IDENT) && peek(parser, 1)->kind == TOK_PERIOD) {
            advance(parser);
            advance(parser);
        }
        GqlNode *n = gql_node_new(GQL_CATALOG_PARENT_REFERENCE);
        gql_node_take_text(n, slice(first, parser->previous_token));
        return n;
    }
    if (at(parser, TOK_IDENT) && peek(parser, 1)->kind == TOK_PERIOD) {
        GqlToken first = *current_token(parser);
        while (!parser->failed && at(parser, TOK_IDENT) && peek(parser, 1)->kind == TOK_PERIOD) {
            advance(parser);
            advance(parser);
        }
        GqlNode *n = gql_node_new(GQL_CATALOG_PARENT_REFERENCE);
        gql_node_take_text(n, slice(first, parser->previous_token));
        return n;
    }
    return NULL;
}

static GqlNode *parse_schema_reference(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_SCHEMA_REFERENCE);
    GqlToken first = *current_token(parser);

    if (at(parser, TOK_PARAM)) {
        advance(parser);
        gql_node_take_text(node, slice(first, parser->previous_token));
        return node;
    }
    if (at(parser, TOK_SOLIDUS)) {
        advance(parser);
        while (!parser->failed && at(parser, TOK_IDENT) && peek(parser, 1)->kind == TOK_SOLIDUS) {
            advance(parser);
            advance(parser);
        }
        if (at(parser, TOK_IDENT)) {
            advance(parser);
        }
        gql_node_take_text(node, slice(first, parser->previous_token));
        return node;
    }
    if (at(parser, TOK_DOUBLE_PERIOD)) {
        advance(parser);
        while (!parser->failed && at(parser, TOK_SOLIDUS) &&
               peek(parser, 1)->kind == TOK_DOUBLE_PERIOD) {
            advance(parser);
            advance(parser);
        }
        match(parser, TOK_SOLIDUS);
        while (!parser->failed && at(parser, TOK_IDENT) && peek(parser, 1)->kind == TOK_SOLIDUS) {
            advance(parser);
            advance(parser);
        }
        if (at(parser, TOK_IDENT)) {
            advance(parser);
        }
        gql_node_take_text(node, slice(first, parser->previous_token));
        return node;
    }
    if (at(parser, TOK_HOME_SCHEMA) || at(parser, TOK_CURRENT_SCHEMA) || at(parser, TOK_PERIOD)) {
        advance(parser);
        gql_node_take_text(node, slice(first, parser->previous_token));
        return node;
    }
    unexpected(parser, "a schema reference");
    gql_node_free(node);
    return NULL;
}

static GqlNode *parse_graph_expression(GqlParser *parser) {
    if (at(parser, TOK_CURRENT_PROPERTY_GRAPH) || at(parser, TOK_CURRENT_GRAPH)) {
        GqlNode *n = gql_node_new(GQL_CURRENT_GRAPH);
        char *s = gql_token_decoded_text(current_token(parser));
        gql_node_take_text(n, s);
        advance(parser);
        return n;
    }
    if (at(parser, TOK_HOME_GRAPH) || at(parser, TOK_HOME_PROPERTY_GRAPH)) {
        GqlNode *n = gql_node_new(GQL_HOME_GRAPH);
        char *s = gql_token_decoded_text(current_token(parser));
        gql_node_take_text(n, s);
        advance(parser);
        return n;
    }
    if (at(parser, TOK_VARIABLE)) {
        advance(parser);
        GqlNode *n = gql_node_new(GQL_GRAPH_EXPR);
        GqlNode *e = parse_value_expression_primary(parser);
        if (e) {
            gql_node_add_child(n, e);
        }
        return n;
    }
    if (at(parser, TOK_SOLIDUS) || at(parser, TOK_DOUBLE_PERIOD) || at(parser, TOK_PARAM) ||
        at(parser, TOK_HOME_SCHEMA) || at(parser, TOK_CURRENT_SCHEMA) ||
        (at(parser, TOK_IDENT) && peek(parser, 1)->kind == TOK_PERIOD)) {
        return parse_graph_reference(parser);
    }
    if (token_is_name(current_token(parser))) {

        return parse_name(parser);
    }
    unexpected(parser, "a graph expression");
    return NULL;
}

static GqlNode *parse_graph_reference(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_GRAPH_REFERENCE);
    GqlNode *parent = parse_catalog_parent_reference(parser);
    GqlNode *name = parse_name(parser);
    if (name) {
        gql_node_set_text(node, name->text);
        gql_node_free(name);
    }
    if (parent) {
        gql_node_add_child(node, parent);
    }
    return node;
}

static GqlNode *parse_binding_table_expression(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_BINDING_TABLE_EXPR);
    if (token_is_name(current_token(parser))) {
        GqlNode *name = parse_name(parser);
        if (name) {
            gql_node_add_child(node, name);
        }
    } else {
        unexpected(parser, "a binding table expression");
        gql_node_free(node);
        return NULL;
    }
    return node;
}

static GqlNode *parse_insert_statement(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_INSERT);
    expect(parser, TOK_INSERT, "INSERT");
    GqlNode *pat = parse_insert_graph_pattern(parser);
    if (pat) {
        gql_node_add_child(node, pat);
    }
    return node;
}

static GqlNode *parse_set_statement(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_SET_STATEMENT);
    expect(parser, TOK_SET, "SET");
    do {
        GqlNode *item = parse_set_item(parser);
        if (item) {
            gql_node_add_child(node, item);
        }
    } while (!parser->failed && match(parser, TOK_COMMA));
    return node;
}

static GqlNode *parse_set_item(GqlParser *parser) {
    GqlNode *item = gql_node_new(GQL_SET_ITEM);
    GqlNode *ref = parse_binding_variable_reference(parser);
    if (ref) {
        gql_node_add_child(item, ref);
    }
    if (match(parser, TOK_PERIOD)) {
        GqlNode *prop = parse_name(parser);
        if (prop) {
            gql_node_add_child(item, prop);
        }
        expect(parser, TOK_EQUALS, "=");
        GqlNode *v = parse_value_expression(parser);
        if (v) {
            gql_node_add_child(item, v);
        }
        item->subkind = GQL_SET_PROPERTY;
    } else if (match(parser, TOK_EQUALS)) {
        expect(parser, TOK_LBRACE, "{");
        item->subkind = GQL_SET_PROPERTIES;
        if (!at(parser, TOK_RBRACE)) {
            do {
                GqlNode *kv = parse_property_key_value_pair(parser);
                if (kv) {
                    gql_node_add_child(item, kv);
                }
            } while (!parser->failed && match(parser, TOK_COMMA));
        }
        expect(parser, TOK_RBRACE, "}");
    } else if (at(parser, TOK_IS) || at(parser, TOK_COLON)) {
        advance(parser);
        item->subkind = GQL_SET_LABEL;
        GqlNode *label = parse_name(parser);
        if (label) {
            gql_node_add_child(item, label);
        }
    } else {
        unexpected(parser, "'.', '=' or ':'");
        gql_node_free(item);
        return NULL;
    }
    return item;
}

static GqlNode *parse_remove_statement(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_REMOVE_STATEMENT);
    expect(parser, TOK_REMOVE, "REMOVE");
    do {
        GqlNode *item = parse_remove_item(parser);
        if (item) {
            gql_node_add_child(node, item);
        }
    } while (!parser->failed && match(parser, TOK_COMMA));
    return node;
}

static GqlNode *parse_remove_item(GqlParser *parser) {
    GqlNode *item = gql_node_new(GQL_REMOVE_ITEM);
    GqlNode *ref = parse_binding_variable_reference(parser);
    if (ref) {
        gql_node_add_child(item, ref);
    }
    if (match(parser, TOK_PERIOD)) {
        item->subkind = GQL_REMOVE_PROPERTY;
        GqlNode *prop = parse_name(parser);
        if (prop) {
            gql_node_add_child(item, prop);
        }
    } else if (at(parser, TOK_IS) || at(parser, TOK_COLON)) {
        advance(parser);
        item->subkind = GQL_REMOVE_LABEL;
        GqlNode *label = parse_name(parser);
        if (label) {
            gql_node_add_child(item, label);
        }
    } else {
        unexpected(parser, "'.' or ':'");
        gql_node_free(item);
        return NULL;
    }
    return item;
}

static GqlNode *parse_delete_statement(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_DELETE_STATEMENT);
    if (match(parser, TOK_DETACH)) {
        node->subkind = GQL_DELETE_DETACH;
    } else if (match(parser, TOK_NODETACH)) {
        node->subkind = GQL_DELETE_NODETACH;
    }
    expect(parser, TOK_DELETE, "DELETE");
    do {
        GqlNode *v = parse_value_expression(parser);
        if (v) {
            gql_node_add_child(node, v);
        }
    } while (!parser->failed && match(parser, TOK_COMMA));
    return node;
}

static GqlNode *parse_match_statement(GqlParser *parser) {
    if (at(parser, TOK_OPTIONAL)) {
        advance(parser);
        GqlNode *node = gql_node_new(GQL_OPTIONAL_MATCH);
        if (at(parser, TOK_MATCH)) {
            advance(parser);
            GqlNode *g = parse_graph_pattern_binding_table(parser);
            if (g) {
                gql_node_add_child(node, g);
            }
        } else if (at(parser, TOK_LBRACE) || at(parser, TOK_LPAREN)) {
            advance(parser);
            while (!parser->failed && (at(parser, TOK_MATCH) || at(parser, TOK_OPTIONAL))) {
                GqlNode *m = parse_match_statement(parser);
                if (m) {
                    gql_node_add_child(node, m);
                }
            }
            expect(parser, at(parser, TOK_LPAREN) ? TOK_RPAREN : TOK_RBRACE, ") or }");
        } else {
            unexpected(parser, "MATCH, '{' or '('");
        }
        return node;
    }

    GqlNode *node = gql_node_new(GQL_MATCH);
    expect(parser, TOK_MATCH, "MATCH");
    GqlNode *g = parse_graph_pattern_binding_table(parser);
    if (g) {
        gql_node_add_child(node, g);
    }
    return node;
}

static GqlNode *parse_graph_pattern_binding_table(GqlParser *parser) {
    GqlNode *g = parse_graph_pattern(parser);
    if (!g) {
        return NULL;
    }
    if (at(parser, TOK_YIELD)) {
        GqlNode *y = parse_yield_clause(parser);
        if (y) {
            gql_node_add_child(g, y);
        }
    }
    return g;
}

static GqlNode *parse_filter_statement(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_FILTER);
    expect(parser, TOK_FILTER, "FILTER");
    GqlNode *cond;
    if (at(parser, TOK_WHERE)) {
        advance(parser);
        cond = gql_node_new(GQL_WHERE);
        GqlNode *e = parse_search_condition(parser);
        if (e) {
            gql_node_add_child(cond, e);
        }
    } else {
        cond = parse_search_condition(parser);
    }
    if (cond) {
        gql_node_add_child(node, cond);
    }
    return node;
}

static GqlNode *parse_let_statement(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_LET);
    expect(parser, TOK_LET, "LET");
    do {
        GqlNode *def = gql_node_new(GQL_VARIABLE_DECL);
        GqlNode *name = parse_name(parser);
        if (name) {
            gql_node_add_child(def, name);
        }
        if (match(parser, TOK_EQUALS)) {
            GqlNode *v = parse_value_expression(parser);
            if (v) {
                gql_node_add_child(def, v);
            }
        }
        gql_node_add_child(node, def);
    } while (!parser->failed && match(parser, TOK_COMMA));
    return node;
}

static GqlNode *parse_for_statement(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_FOR);
    expect(parser, TOK_FOR, "FOR");
    GqlNode *alias = parse_name(parser);
    if (alias) {
        gql_node_add_child(node, alias);
    }
    expect(parser, TOK_IN, "IN");
    GqlNode *source = parse_value_expression(parser);
    if (source) {
        gql_node_add_child(node, source);
    }
    if (at(parser, TOK_WITH)) {
        advance(parser);
        GqlNode *ord = gql_node_new(GQL_NAME);
        if (match(parser, TOK_ORDINALITY)) {
            gql_node_set_text(ord, "ORDINALITY");
        } else if (match(parser, TOK_OFFSET)) {
            gql_node_set_text(ord, "OFFSET");
        }
        GqlNode *v = parse_name(parser);
        if (v) {
            gql_node_add_child(ord, v);
        }
        gql_node_add_child(node, ord);
    }
    return node;
}

static GqlNode *parse_order_by_and_page(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_ORDER_BY_PAGE);
    if (at(parser, TOK_ORDER)) {
        GqlNode *o = parse_order_by_clause(parser);
        if (o) {
            gql_node_add_child(node, o);
        }
    }
    if (at(parser, TOK_OFFSET) || at(parser, TOK_SKIP_RESERVED_WORD)) {
        GqlNode *o = parse_offset_clause(parser);
        if (o) {
            gql_node_add_child(node, o);
        }
    }
    if (at(parser, TOK_LIMIT)) {
        GqlNode *o = parse_limit_clause(parser);
        if (o) {
            gql_node_add_child(node, o);
        }
    }
    return node;
}

static GqlNode *parse_return_statement(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_RETURN);
    expect(parser, TOK_RETURN, "RETURN");

    if (match(parser, TOK_DISTINCT)) {
        node->subkind = GQL_SQ_DISTINCT;
    } else if (match(parser, TOK_ALL)) {
        node->subkind = GQL_SQ_ALL;
    }

    if (match(parser, TOK_ASTERISK)) {
        GqlNode *star = gql_node_new(GQL_NAME);
        gql_node_set_text(star, "*");
        gql_node_add_child(node, star);
    } else {
        do {
            GqlNode *expr = parse_value_expression(parser);
            if (!expr) {
                break;
            }
            if (at(parser, TOK_AS)) {
                advance(parser);
                GqlNode *alias = gql_node_new(GQL_FIELD);
                GqlNode *nm = parse_name(parser);
                if (nm) {
                    gql_node_set_text(alias, nm->text);
                    gql_node_free(nm);
                }
                gql_node_add_child(alias, expr);
                gql_node_add_child(node, alias);
            } else {
                gql_node_add_child(node, expr);
            }
        } while (!parser->failed && match(parser, TOK_COMMA));
    }
    if (at(parser, TOK_GROUP)) {
        GqlNode *g = parse_group_by_clause(parser);
        if (g) {
            gql_node_add_child(node, g);
        }
    }
    return node;
}

static GqlNode *parse_select_statement(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_SELECT);
    expect(parser, TOK_SELECT, "SELECT");
    if (match(parser, TOK_DISTINCT)) {
        node->subkind = GQL_SQ_DISTINCT;
    } else if (match(parser, TOK_ALL)) {
        node->subkind = GQL_SQ_ALL;
    }

    if (match(parser, TOK_ASTERISK)) {
        GqlNode *star = gql_node_new(GQL_NAME);
        gql_node_set_text(star, "*");
        gql_node_add_child(node, star);
    } else {
        do {
            GqlNode *expr = parse_value_expression(parser);
            if (!expr) {
                break;
            }
            if (at(parser, TOK_AS)) {
                advance(parser);
                GqlNode *alias = gql_node_new(GQL_FIELD);
                GqlNode *nm = parse_name(parser);
                if (nm) {
                    gql_node_set_text(alias, nm->text);
                    gql_node_free(nm);
                }
                gql_node_add_child(alias, expr);
                gql_node_add_child(node, alias);
            } else {
                gql_node_add_child(node, expr);
            }
        } while (!parser->failed && match(parser, TOK_COMMA));
    }

    if (at(parser, TOK_FROM)) {
        advance(parser);

        GqlNode *body = gql_node_new(GQL_GRAPH_PATTERN);
        if (at(parser, TOK_MATCH) || at(parser, TOK_OPTIONAL)) {
            GqlNode *m = parse_match_statement(parser);
            if (m) {
                gql_node_add_child(body, m);
            }
        } else {
            GqlNode *g = parse_graph_expression(parser);
            if (g) {
                gql_node_add_child(body, g);
            }
            if (at(parser, TOK_MATCH) || at(parser, TOK_OPTIONAL)) {
                GqlNode *m = parse_match_statement(parser);
                if (m) {
                    gql_node_add_child(body, m);
                }
            }
        }
        gql_node_add_child(node, body);
    }
    if (at(parser, TOK_WHERE)) {
        GqlNode *w = gql_node_new(GQL_WHERE);
        advance(parser);
        GqlNode *e = parse_search_condition(parser);
        if (e) {
            gql_node_add_child(w, e);
        }
        gql_node_add_child(node, w);
    }
    if (at(parser, TOK_GROUP)) {
        GqlNode *g = parse_group_by_clause(parser);
        if (g) {
            gql_node_add_child(node, g);
        }
    }
    if (at(parser, TOK_HAVING)) {
        GqlNode *h = gql_node_new(GQL_HAVING);
        advance(parser);
        GqlNode *e = parse_search_condition(parser);
        if (e) {
            gql_node_add_child(h, e);
        }
        gql_node_add_child(node, h);
    }
    if (at(parser, TOK_ORDER)) {
        GqlNode *o = parse_order_by_clause(parser);
        if (o) {
            gql_node_add_child(node, o);
        }
    }
    if (at(parser, TOK_OFFSET) || at(parser, TOK_SKIP_RESERVED_WORD)) {
        GqlNode *o = parse_offset_clause(parser);
        if (o) {
            gql_node_add_child(node, o);
        }
    }
    if (at(parser, TOK_LIMIT)) {
        GqlNode *o = parse_limit_clause(parser);
        if (o) {
            gql_node_add_child(node, o);
        }
    }
    return node;
}

static GqlNode *parse_call_statement(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_CALL_PROCEDURE);
    int optional = 0;
    if (match(parser, TOK_OPTIONAL)) {
        optional = 1;
    }
    expect(parser, TOK_CALL, "CALL");
    node->integer_value = optional;
    GqlNode *name = parse_name(parser);
    if (name) {
        gql_node_add_child(node, name);
    }
    if (match(parser, TOK_LPAREN)) {
        if (!at(parser, TOK_RPAREN)) {
            do {
                GqlNode *a = parse_value_expression(parser);
                if (a) {
                    gql_node_add_child(node, a);
                }
            } while (!parser->failed && match(parser, TOK_COMMA));
        }
        expect(parser, TOK_RPAREN, ")");
    }
    if (at(parser, TOK_YIELD)) {
        GqlNode *y = parse_yield_clause(parser);
        if (y) {
            gql_node_add_child(node, y);
        }
    }
    return node;
}

static GqlNode *parse_graph_pattern(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_GRAPH_PATTERN);

    if (at(parser, TOK_REPEATABLE) || at(parser, TOK_DIFFERENT)) {
        advance(parser);
        while (!parser->failed &&
               (at(parser, TOK_ELEMENT) || at(parser, TOK_ELEMENTS) || at(parser, TOK_EDGE) ||
                at(parser, TOK_EDGES) || at(parser, TOK_RELATIONSHIP) ||
                at(parser, TOK_RELATIONSHIPS) || at(parser, TOK_BINDINGS))) {
            advance(parser);
        }
    }

    do {
        GqlNode *pp = parse_path_pattern(parser);
        if (!pp) {
            break;
        }
        gql_node_add_child(node, pp);
    } while (!parser->failed && match(parser, TOK_COMMA));

    if (at(parser, TOK_KEEP)) {
        advance(parser);
        GqlNode *keep = gql_node_new(GQL_KEEP);

        while (!parser->failed &&
               (at(parser, TOK_WALK) || at(parser, TOK_TRAIL) || at(parser, TOK_SIMPLE) ||
                at(parser, TOK_ACYCLIC) || at(parser, TOK_ALL) || at(parser, TOK_ANY) ||
                at(parser, TOK_SHORTEST) || at(parser, TOK_PATH) || at(parser, TOK_PATHS))) {
            advance(parser);
        }
        gql_node_add_child(node, keep);
    }

    if (at(parser, TOK_WHERE)) {
        advance(parser);
        GqlNode *w = gql_node_new(GQL_WHERE);
        GqlNode *e = parse_search_condition(parser);
        if (e) {
            gql_node_add_child(w, e);
        }
        gql_node_add_child(node, w);
    }
    return node;
}

static GqlNode *parse_path_pattern(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_PATH_PATTERN);

    if (token_is_name(current_token(parser)) && peek(parser, 1)->kind == TOK_EQUALS) {
        GqlNode *var = parse_name(parser);
        if (var) {
            gql_node_add_child(node, var);
        }
        advance(parser);
    }

    if (at(parser, TOK_WALK) || at(parser, TOK_TRAIL) || at(parser, TOK_SIMPLE) ||
        at(parser, TOK_ACYCLIC) || at(parser, TOK_ALL) || at(parser, TOK_ANY) ||
        at(parser, TOK_SHORTEST)) {
        GqlNode *prefix = gql_node_new(GQL_NAME);
        gql_node_set_text(prefix, "path_prefix");
        while (!parser->failed &&
               (at(parser, TOK_WALK) || at(parser, TOK_TRAIL) || at(parser, TOK_SIMPLE) ||
                at(parser, TOK_ACYCLIC) || at(parser, TOK_ALL) || at(parser, TOK_ANY) ||
                at(parser, TOK_SHORTEST) || at(parser, TOK_PATH) || at(parser, TOK_PATHS) ||
                at(parser, TOK_GROUP) || at(parser, TOK_GROUPS) || at(parser, TOK_INT_LIT))) {
            advance(parser);
        }
        gql_node_add_child(node, prefix);
    }
    GqlNode *expr = parse_path_pattern_expression(parser);
    if (expr) {
        gql_node_add_child(node, expr);
    }
    return node;
}

static GqlNode *parse_path_pattern_expression(GqlParser *parser) {
    GqlNode *term = parse_path_term(parser);
    if (!term) {
        return NULL;
    }
    if (at(parser, TOK_MULTISET_ALTERNATION)) {
        GqlNode *node = gql_node_new(GQL_PATH_EXPRESSION);
        node->subkind = GQL_PATH_MULTISET_ALTERNATION;
        gql_node_add_child(node, term);
        while (!parser->failed && match(parser, TOK_MULTISET_ALTERNATION)) {
            GqlNode *t = parse_path_term(parser);
            if (t) {
                gql_node_add_child(node, t);
            }
        }
        return node;
    }
    if (at(parser, TOK_VERTICAL_BAR)) {
        GqlNode *node = gql_node_new(GQL_PATH_EXPRESSION);
        node->subkind = GQL_PATH_ALTERNATION;
        gql_node_add_child(node, term);
        while (!parser->failed && match(parser, TOK_VERTICAL_BAR)) {
            GqlNode *t = parse_path_term(parser);
            if (t) {
                gql_node_add_child(node, t);
            }
        }
        return node;
    }
    return term;
}

static int edge_pattern_start(GqlTokenKind k) {
    switch (k) {
    case TOK_MINUS_LEFT_BRACKET:
    case TOK_LEFT_ARROW_BRACKET:
    case TOK_TILDE_LEFT_BRACKET:
    case TOK_LEFT_ARROW_TILDE_BRACKET:
    case TOK_LEFT_ARROW:
    case TOK_TILDE:
    case TOK_RIGHT_ARROW:
    case TOK_LEFT_ARROW_TILDE:
    case TOK_TILDE_RIGHT_ARROW:
    case TOK_LEFT_MINUS_RIGHT:
    case TOK_MINUS:
        return 1;
    default:
        return 0;
    }
}

static GqlNode *parse_path_term(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_PATH_EXPRESSION);
    while (!parser->failed &&
           (at(parser, TOK_LPAREN) || edge_pattern_start(current_token(parser)->kind) ||
            at(parser, TOK_MINUS_SLASH) || at(parser, TOK_LEFT_MINUS_SLASH) ||
            at(parser, TOK_TILDE_SLASH) || at(parser, TOK_LEFT_TILDE_SLASH))) {
        GqlNode *factor = parse_path_factor(parser);
        if (!factor) {
            break;
        }
        gql_node_add_child(node, factor);
    }
    if (node->child_count == 0) {
        gql_node_free(node);
        unexpected(parser, "a path pattern");
        return NULL;
    }
    return node;
}

static GqlNode *parse_path_factor(GqlParser *parser) {
    GqlNode *primary = parse_path_primary(parser);
    if (!primary) {
        return NULL;
    }
    if (at(parser, TOK_ASTERISK) || at(parser, TOK_PLUS) || at(parser, TOK_QUESTION_MARK) ||
        at(parser, TOK_LBRACE)) {
        GqlNode *q = parse_graph_pattern_quantifier(parser);
        if (q) {
            gql_node_add_child(primary, q);
        }
    }
    return primary;
}

static GqlNode *parse_path_primary(GqlParser *parser) {
    if (at(parser, TOK_LPAREN)) {
        GqlTokenKind k = peek(parser, 1)->kind;
        if (k == TOK_RPAREN || token_is_name(peek(parser, 1)) || k == TOK_COLON || k == TOK_IS ||
            k == TOK_WHERE || k == TOK_LBRACE) {
            return parse_node_pattern(parser);
        }
        return parse_parenthesized_path(parser);
    }
    if (edge_pattern_start(current_token(parser)->kind)) {
        return parse_edge_pattern(parser);
    }
    if (at(parser, TOK_MINUS_SLASH) || at(parser, TOK_LEFT_MINUS_SLASH) ||
        at(parser, TOK_TILDE_SLASH) || at(parser, TOK_LEFT_TILDE_SLASH)) {
        return parse_simplified_path(parser);
    }
    unexpected(parser, "a node or edge pattern");
    return NULL;
}

static GqlNode *parse_node_pattern(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_NODE_PATTERN);
    expect(parser, TOK_LPAREN, "(");
    if (!at(parser, TOK_RPAREN)) {
        GqlNode *filler = parse_element_filler(parser, 0);
        if (filler) {
            gql_node_add_child(node, filler);
        }
    }
    expect(parser, TOK_RPAREN, ")");
    return node;
}

static int is_edge_close(GqlTokenKind k) {
    return k == TOK_RIGHT_BRACKET_MINUS || k == TOK_BRACKET_RIGHT_ARROW ||
           k == TOK_RIGHT_BRACKET_TILDE || k == TOK_BRACKET_TILDE_RIGHT_ARROW;
}

static int edge_direction(GqlTokenKind open, GqlTokenKind close) {
    if (open == TOK_LEFT_ARROW_BRACKET) {
        if (close == TOK_RIGHT_BRACKET_MINUS) {
            return GQL_DIR_LEFT;
        }
        if (close == TOK_BRACKET_RIGHT_ARROW) {
            return GQL_DIR_LEFT_OR_RIGHT;
        }
    } else if (open == TOK_TILDE_LEFT_BRACKET) {
        if (close == TOK_RIGHT_BRACKET_TILDE) {
            return GQL_DIR_UNDIRECTED;
        }
        if (close == TOK_BRACKET_TILDE_RIGHT_ARROW) {
            return GQL_DIR_UNDIRECTED_OR_RIGHT;
        }
    } else if (open == TOK_MINUS_LEFT_BRACKET) {
        if (close == TOK_BRACKET_RIGHT_ARROW) {
            return GQL_DIR_RIGHT;
        }
        if (close == TOK_RIGHT_BRACKET_MINUS) {
            return GQL_DIR_ANY;
        }
    } else if (open == TOK_LEFT_ARROW_TILDE_BRACKET) {
        if (close == TOK_RIGHT_BRACKET_TILDE) {
            return GQL_DIR_LEFT_OR_UNDIRECTED;
        }
    }
    return GQL_DIR_NONE;
}

static GqlNode *parse_edge_pattern(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_EDGE_PATTERN);
    GqlTokenKind k = current_token(parser)->kind;

    switch (k) {
    case TOK_LEFT_ARROW:
        advance(parser);
        node->subkind = GQL_DIR_LEFT;
        return node;
    case TOK_TILDE:
        advance(parser);
        node->subkind = GQL_DIR_UNDIRECTED;
        return node;
    case TOK_RIGHT_ARROW:
        advance(parser);
        node->subkind = GQL_DIR_RIGHT;
        return node;
    case TOK_LEFT_ARROW_TILDE:
        advance(parser);
        node->subkind = GQL_DIR_LEFT_OR_UNDIRECTED;
        return node;
    case TOK_TILDE_RIGHT_ARROW:
        advance(parser);
        node->subkind = GQL_DIR_UNDIRECTED_OR_RIGHT;
        return node;
    case TOK_LEFT_MINUS_RIGHT:
        advance(parser);
        node->subkind = GQL_DIR_LEFT_OR_RIGHT;
        return node;
    case TOK_MINUS:
        advance(parser);
        node->subkind = GQL_DIR_ANY;
        return node;
    default:
        break;
    }

    GqlTokenKind open = k;
    advance(parser);
    if (!is_edge_close(current_token(parser)->kind)) {
        GqlNode *filler = parse_element_filler(parser, 0);
        if (filler) {
            gql_node_add_child(node, filler);
        }
    }
    GqlTokenKind close = current_token(parser)->kind;
    GqlDirection dir = (GqlDirection)edge_direction(open, close);
    if (dir == GQL_DIR_NONE) {
        unexpected(parser, "a matching edge pattern closing bracket");
        gql_node_free(node);
        return NULL;
    }
    node->subkind = dir;
    advance(parser);
    return node;
}

static GqlNode *parse_element_filler(GqlParser *parser, int insert) {
    GqlNode *node = gql_node_new(GQL_ELEMENT_FILLER);

    if (token_is_name(current_token(parser))) {
        GqlNode *var = parse_name(parser);
        if (var) {
            gql_node_add_child(node, var);
        }
    }

    if (at(parser, TOK_COLON) || at(parser, TOK_IS)) {
        advance(parser);
        GqlNode *label;
        if (insert) {
            label = parse_label_set_specification(parser);
        } else {
            label = parse_label_expression(parser);
        }
        if (label) {
            gql_node_add_child(node, label);
        }
    }

    if (at(parser, TOK_WHERE)) {
        advance(parser);
        GqlNode *w = gql_node_new(GQL_WHERE);
        GqlNode *e = parse_search_condition(parser);
        if (e) {
            gql_node_add_child(w, e);
        }
        gql_node_add_child(node, w);
    } else if (at(parser, TOK_LBRACE)) {
        advance(parser);
        if (!at(parser, TOK_RBRACE)) {
            do {
                GqlNode *kv = parse_property_key_value_pair(parser);
                if (kv) {
                    gql_node_add_child(node, kv);
                }
            } while (!parser->failed && match(parser, TOK_COMMA));
        }
        expect(parser, TOK_RBRACE, "}");
    }
    return node;
}

static GqlNode *parse_parenthesized_path(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_PATH_EXPRESSION);
    node->subkind = GQL_PATH_PARENTHESIZED;
    expect(parser, TOK_LPAREN, "(");
    while (!parser->failed && !at(parser, TOK_RPAREN)) {
        if (at(parser, TOK_LPAREN) || edge_pattern_start(current_token(parser)->kind)) {
            GqlNode *f = parse_path_factor(parser);
            if (f) {
                gql_node_add_child(node, f);
            }
        } else {
            advance(parser);
        }
    }
    expect(parser, TOK_RPAREN, ")");
    return node;
}

static GqlNode *parse_simplified_path(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_PATH_EXPRESSION);
    node->subkind = GQL_PATH_SIMPLIFIED;
    GqlTokenKind open = current_token(parser)->kind;
    GqlTokenKind close;
    switch (open) {
    case TOK_LEFT_MINUS_SLASH:
        close = TOK_SLASH_MINUS;
        break;
    case TOK_TILDE_SLASH:
        close = TOK_SLASH_TILDE;
        break;
    case TOK_MINUS_SLASH:
        close = TOK_SLASH_MINUS_RIGHT;
        break;
    case TOK_LEFT_TILDE_SLASH:
        close = TOK_SLASH_TILDE;
        break;
    default:
        close = TOK_SLASH_MINUS;
        break;
    }
    advance(parser);
    int depth = 0;
    while (!parser->failed) {
        if (at(parser, TOK_LPAREN)) {
            depth++;
        } else if (at(parser, TOK_RPAREN)) {
            depth--;
        } else if (at(parser, close) && depth == 0) {
            advance(parser);
            break;
        } else if (at(parser, TOK_EOF)) {
            break;
        }
        advance(parser);
    }
    return node;
}

static GqlNode *parse_label_expression(GqlParser *parser) {
    GqlNode *node = parse_label_unary(parser);
    if (!node) {
        return NULL;
    }

    while (!parser->failed && at(parser, TOK_AMPERSAND)) {
        advance(parser);
        GqlNode *rhs = parse_label_unary(parser);
        if (!rhs) {
            break;
        }
        GqlNode *conj = gql_node_new(GQL_LABEL_EXPRESSION);
        conj->subkind = GQL_LABEL_CONJUNCTION;
        gql_node_add_child(conj, node);
        gql_node_add_child(conj, rhs);
        node = conj;
    }

    if (at(parser, TOK_VERTICAL_BAR)) {
        GqlNode *disj = gql_node_new(GQL_LABEL_EXPRESSION);
        disj->subkind = GQL_LABEL_DISJUNCTION;
        gql_node_add_child(disj, node);
        while (!parser->failed && match(parser, TOK_VERTICAL_BAR)) {
            GqlNode *rhs = parse_label_unary(parser);
            if (!rhs) {
                break;
            }
            GqlNode *conj = rhs;

            gql_node_add_child(disj, conj);
        }
        node = disj;
    }
    return node;
}

static GqlNode *parse_label_unary(GqlParser *parser) {
    if (at(parser, TOK_EXCLAMATION_MARK)) {
        advance(parser);
        GqlNode *n = gql_node_new(GQL_LABEL_EXPRESSION);
        n->subkind = GQL_LABEL_NEGATION;
        GqlNode *operand = parse_label_unary(parser);
        if (operand) {
            gql_node_add_child(n, operand);
        }
        return n;
    }
    if (at(parser, TOK_PERCENT)) {
        advance(parser);
        GqlNode *n = gql_node_new(GQL_LABEL_EXPRESSION);
        n->subkind = GQL_LABEL_WILDCARD;
        return n;
    }
    if (at(parser, TOK_LPAREN)) {
        advance(parser);
        GqlNode *inner = parse_label_expression(parser);
        expect(parser, TOK_RPAREN, ")");
        return inner;
    }
    if (token_is_name(current_token(parser))) {
        GqlNode *n = gql_node_new(GQL_LABEL_EXPRESSION);
        n->subkind = GQL_LABEL_NAME;
        GqlNode *name = parse_name(parser);
        if (name) {
            gql_node_add_child(n, name);
        }
        return n;
    }
    unexpected(parser, "a label expression");
    return NULL;
}

static GqlNode *parse_label_set_specification(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_LABEL_EXPRESSION);
    node->subkind = GQL_LABEL_CONJUNCTION;
    GqlNode *name = parse_name(parser);
    if (name) {
        gql_node_add_child(node, name);
    }
    while (!parser->failed && match(parser, TOK_AMPERSAND)) {
        GqlNode *name2 = parse_name(parser);
        if (name2) {
            gql_node_add_child(node, name2);
        }
    }
    return node;
}

static GqlNode *parse_graph_pattern_quantifier(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_QUANTIFIER);
    if (match(parser, TOK_ASTERISK)) {
        node->subkind = GQL_QUANT_STAR;
        return node;
    }
    if (match(parser, TOK_PLUS)) {
        node->subkind = GQL_QUANT_PLUS;
        return node;
    }
    if (match(parser, TOK_QUESTION_MARK)) {
        node->subkind = GQL_QUANT_QUESTION;
        return node;
    }
    if (match(parser, TOK_LBRACE)) {
        GqlNode *lo = NULL, *hi = NULL;
        if (at(parser, TOK_INT_LIT)) {
            lo = parse_non_negative_integer(parser);
        }
        if (match(parser, TOK_COMMA)) {
            if (at(parser, TOK_INT_LIT)) {
                hi = parse_non_negative_integer(parser);
            }
            node->subkind = GQL_QUANT_GENERAL;
            if (lo) {
                gql_node_add_child(node, lo);
            }
            if (hi) {
                gql_node_add_child(node, hi);
            }
        } else {
            node->subkind = GQL_QUANT_FIXED;
            if (lo) {
                gql_node_add_child(node, lo);
            }
        }
        expect(parser, TOK_RBRACE, "}");
        return node;
    }
    unexpected(parser, "a path quantifier");
    gql_node_free(node);
    return NULL;
}

static GqlNode *parse_insert_graph_pattern(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_INSERT_GRAPH_PATTERN);
    do {
        GqlNode *path = parse_insert_path_pattern(parser);
        if (!path) {
            break;
        }
        gql_node_add_child(node, path);
    } while (!parser->failed && match(parser, TOK_COMMA));
    return node;
}

static GqlNode *parse_insert_path_pattern(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_INSERT_PATH_PATTERN);
    GqlNode *first = parse_insert_node_pattern(parser);
    if (first) {
        gql_node_add_child(node, first);
    }
    while (!parser->failed && edge_pattern_start(current_token(parser)->kind)) {
        GqlNode *edge = parse_insert_edge_pattern(parser);
        if (!edge) {
            break;
        }
        gql_node_add_child(node, edge);
        GqlNode *n = parse_insert_node_pattern(parser);
        if (n) {
            gql_node_add_child(node, n);
        }
    }
    return node;
}

static GqlNode *parse_insert_node_pattern(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_INSERT_NODE_PATTERN);
    expect(parser, TOK_LPAREN, "(");
    if (!at(parser, TOK_RPAREN)) {
        GqlNode *filler = parse_element_filler(parser, 1);
        if (filler) {
            gql_node_add_child(node, filler);
        }
    }
    expect(parser, TOK_RPAREN, ")");
    return node;
}

static GqlNode *parse_insert_edge_pattern(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_INSERT_EDGE_PATTERN);
    GqlTokenKind k = current_token(parser)->kind;
    GqlTokenKind close;
    switch (k) {
    case TOK_LEFT_ARROW_BRACKET:
        node->subkind = GQL_DIR_LEFT;
        close = TOK_RIGHT_BRACKET_MINUS;
        break;
    case TOK_MINUS_LEFT_BRACKET:
        node->subkind = GQL_DIR_RIGHT;
        close = TOK_BRACKET_RIGHT_ARROW;
        break;
    case TOK_TILDE_LEFT_BRACKET:
        node->subkind = GQL_DIR_UNDIRECTED;
        close = TOK_RIGHT_BRACKET_TILDE;
        break;
    default:
        unexpected(parser, "an insert edge pattern");
        gql_node_free(node);
        return NULL;
    }
    advance(parser);
    if (!at(parser, close)) {
        GqlNode *filler = parse_element_filler(parser, 1);
        if (filler) {
            gql_node_add_child(node, filler);
        }
    }
    expect(parser, close, "an edge pattern closing bracket");
    return node;
}

static GqlNode *parse_nested_graph_type(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_NESTED_GRAPH_TYPE);
    expect(parser, TOK_LBRACE, "{");
    GqlNode *list = gql_node_new(GQL_ELEMENT_TYPE_LIST);
    if (!at(parser, TOK_RBRACE)) {
        do {
            GqlNode *elem = parse_element_type_specification(parser);
            if (elem) {
                gql_node_add_child(list, elem);
            }
        } while (!parser->failed && match(parser, TOK_COMMA));
    }
    gql_node_add_child(node, list);
    expect(parser, TOK_RBRACE, "}");
    return node;
}

static GqlNode *parse_element_type_specification(GqlParser *parser) {

    if (at(parser, TOK_DIRECTED) || at(parser, TOK_UNDIRECTED) || at(parser, TOK_EDGE) ||
        at(parser, TOK_RELATIONSHIP)) {
        return parse_edge_type_specification(parser);
    }
    return parse_node_type_specification(parser);
}

static GqlNode *parse_node_type_specification(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_NODE_TYPE);

    if (at(parser, TOK_NODE) || at(parser, TOK_VERTEX)) {
        advance(parser);
        match(parser, TOK_TYPE);
        if (token_is_name(current_token(parser))) {
            GqlNode *name = parse_name(parser);
            if (name) {
                gql_node_set_text(node, name->text);
                gql_node_free(name);
            }
        }
    } else if (token_is_name(current_token(parser)) && peek(parser, 1)->kind == TOK_LPAREN) {

        GqlNode *name = parse_name(parser);
        if (name) {
            gql_node_set_text(node, name->text);
            gql_node_free(name);
        }
    }
    expect(parser, TOK_LPAREN, "(");

    if (token_is_name(current_token(parser)) && peek(parser, 1)->kind != TOK_RPAREN) {
        GqlNode *alias = parse_name(parser);
        if (alias) {
            gql_node_add_child(node, alias);
        }
    }

    if (at(parser, TOK_LABEL) || at(parser, TOK_LABELS) || at(parser, TOK_COLON) ||
        at(parser, TOK_IS)) {
        GqlNode *labels = parse_label_set_phrase(parser);
        if (labels) {
            gql_node_add_child(node, labels);
        }
    }
    match(parser, TOK_IMPLIES);
    match(parser, TOK_RIGHT_DOUBLE_ARROW);
    if (at(parser, TOK_LBRACE)) {
        GqlNode *props = parse_property_types_specification(parser);
        if (props) {
            gql_node_add_child(node, props);
        }
    }
    expect(parser, TOK_RPAREN, ")");
    return node;
}

static GqlNode *parse_edge_type_specification(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_EDGE_TYPE);

    if (at(parser, TOK_DIRECTED) || at(parser, TOK_UNDIRECTED)) {
        advance(parser);
    }
    if (at(parser, TOK_EDGE) || at(parser, TOK_RELATIONSHIP)) {
        advance(parser);
        match(parser, TOK_TYPE);
        if (token_is_name(current_token(parser))) {
            GqlNode *name = parse_name(parser);
            if (name) {
                gql_node_set_text(node, name->text);
                gql_node_free(name);
            }
        }
    }

    if (at(parser, TOK_LPAREN)) {
        GqlNode *source = parse_node_type_endpoint(parser);
        if (source) {
            gql_node_add_child(node, source);
        }
    }
    GqlNode *filler = NULL;
    if (at(parser, TOK_MINUS_LEFT_BRACKET) || at(parser, TOK_LEFT_ARROW_BRACKET) ||
        at(parser, TOK_TILDE_LEFT_BRACKET)) {
        advance(parser);
        GqlNode *f = parse_edge_type_filler(parser);
        filler = f;

        if (at(parser, TOK_BRACKET_RIGHT_ARROW)) {
            advance(parser);
        } else if (at(parser, TOK_RIGHT_BRACKET_MINUS)) {
            advance(parser);
        } else if (at(parser, TOK_RIGHT_BRACKET_TILDE)) {
            advance(parser);
        }
    }
    if (filler) {
        gql_node_add_child(node, filler);
    }
    if (at(parser, TOK_LPAREN)) {
        GqlNode *destination = parse_node_type_endpoint(parser);
        if (destination) {
            gql_node_add_child(node, destination);
        }
    }
    return node;
}

static GqlNode *parse_node_type_endpoint(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_NODE_TYPE);
    expect(parser, TOK_LPAREN, "(");
    if (token_is_name(current_token(parser))) {
        GqlNode *alias = parse_name(parser);
        if (alias) {
            gql_node_add_child(node, alias);
        }
    } else if (at(parser, TOK_COLON) || at(parser, TOK_IS)) {
        advance(parser);
        GqlNode *labels = parse_label_set_specification(parser);
        if (labels) {
            gql_node_add_child(node, labels);
        }
    }
    expect(parser, TOK_RPAREN, ")");
    return node;
}

static GqlNode *parse_edge_type_filler(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_ELEMENT_FILLER);
    if (at(parser, TOK_LABEL) || at(parser, TOK_LABELS) || at(parser, TOK_COLON) ||
        at(parser, TOK_IS)) {
        GqlNode *labels = parse_label_set_phrase(parser);
        if (labels) {
            gql_node_add_child(node, labels);
        }
    }
    match(parser, TOK_IMPLIES);
    match(parser, TOK_RIGHT_DOUBLE_ARROW);
    if (at(parser, TOK_LBRACE)) {
        GqlNode *props = parse_property_types_specification(parser);
        if (props) {
            gql_node_add_child(node, props);
        }
    }
    return node;
}

static GqlNode *parse_label_set_phrase(GqlParser *parser) {
    if (match(parser, TOK_LABEL)) {
        GqlNode *node = gql_node_new(GQL_LABEL_EXPRESSION);
        node->subkind = GQL_LABEL_NAME;
        GqlNode *name = parse_name(parser);
        if (name) {
            gql_node_add_child(node, name);
        }
        return node;
    }
    if (match(parser, TOK_LABELS)) {
        return parse_label_set_specification(parser);
    }
    if (match(parser, TOK_COLON) || match(parser, TOK_IS)) {
        return parse_label_set_specification(parser);
    }
    unexpected(parser, "a label set");
    return NULL;
}

static GqlNode *parse_property_types_specification(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_PROPERTY_TYPE_LIST);
    expect(parser, TOK_LBRACE, "{");
    if (!at(parser, TOK_RBRACE)) {
        do {
            GqlNode *prop = gql_node_new(GQL_PROPERTY_TYPE);
            GqlNode *name = parse_name(parser);
            if (name) {
                gql_node_set_text(prop, name->text);
                gql_node_free(name);
            }

            GqlNode *typed = parse_typed_optional(parser);
            if (typed) {
                gql_node_add_child(prop, typed);
            }
            GqlNode *type = parse_value_type(parser);
            if (type) {
                gql_node_add_child(prop, type);
            }
            gql_node_add_child(node, prop);
        } while (!parser->failed && match(parser, TOK_COMMA));
    }
    expect(parser, TOK_RBRACE, "}");
    return node;
}

static int is_predefined_type_token(GqlTokenKind k) {
    switch (k) {
    case TOK_BOOL:
    case TOK_BOOLEAN:
    case TOK_STRING:
    case TOK_CHAR:
    case TOK_VARCHAR:
    case TOK_BYTES:
    case TOK_BINARY:
    case TOK_VARBINARY:
    case TOK_INT8:
    case TOK_INT16:
    case TOK_INT32:
    case TOK_INT64:
    case TOK_INT128:
    case TOK_INT256:
    case TOK_INTEGER8:
    case TOK_INTEGER16:
    case TOK_INTEGER32:
    case TOK_INTEGER64:
    case TOK_INTEGER128:
    case TOK_INTEGER256:
    case TOK_SMALLINT:
    case TOK_INT:
    case TOK_INTEGER:
    case TOK_BIGINT:
    case TOK_UINT8:
    case TOK_UINT16:
    case TOK_UINT32:
    case TOK_UINT64:
    case TOK_UINT128:
    case TOK_UINT256:
    case TOK_USMALLINT:
    case TOK_UINT:
    case TOK_UBIGINT:
    case TOK_DECIMAL:
    case TOK_DEC:
    case TOK_FLOAT16:
    case TOK_FLOAT32:
    case TOK_FLOAT64:
    case TOK_FLOAT128:
    case TOK_FLOAT256:
    case TOK_FLOAT:
    case TOK_REAL:
    case TOK_DOUBLE:
    case TOK_DATE:
    case TOK_DATETIME:
    case TOK_TIMESTAMP:
    case TOK_TIME:
    case TOK_DURATION:
        return 1;
    default:
        return 0;
    }
}

static GqlNode *parse_value_type(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_TYPE);

    if (at(parser, TOK_ANY)) {
        advance(parser);
        if (at(parser, TOK_VALUE)) {
            advance(parser);
            node->subkind = GQL_TYPE_OPEN_UNION;
        } else if (at(parser, TOK_PROPERTY) || at(parser, TOK_GRAPH)) {
            match(parser, TOK_PROPERTY);
            advance(parser);
            node->subkind = GQL_TYPE_GRAPH_REF;
        } else if (at(parser, TOK_NODE) || at(parser, TOK_VERTEX)) {
            advance(parser);
            node->subkind = GQL_TYPE_NODE_REF;
        } else if (at(parser, TOK_EDGE) || at(parser, TOK_RELATIONSHIP)) {
            advance(parser);
            node->subkind = GQL_TYPE_EDGE_REF;
        } else if (at(parser, TOK_RECORD)) {
            advance(parser);
            node->subkind = GQL_TYPE_RECORD;
        } else {
            node->subkind = GQL_TYPE_OPEN_UNION;
        }
        parse_not_null(parser, node);
        return node;
    }
    if (at(parser, TOK_LIST) || at(parser, TOK_ARRAY)) {
        advance(parser);
        node->subkind = GQL_TYPE_LIST;
        if (match(parser, TOK_LEFT_ANGLE)) {
            GqlNode *elem = parse_value_type(parser);
            if (elem) {
                gql_node_add_child(node, elem);
            }
            expect(parser, TOK_RIGHT_ANGLE, ">");
        }
        parse_not_null(parser, node);
        return node;
    }
    if (at(parser, TOK_PATH)) {
        advance(parser);
        node->subkind = GQL_TYPE_PATH;
        gql_node_set_text(node, "PATH");
        parse_not_null(parser, node);
        return node;
    }
    if (at(parser, TOK_RECORD) || at(parser, TOK_LBRACE)) {
        match(parser, TOK_RECORD);
        node->subkind = GQL_TYPE_RECORD;

        if (at(parser, TOK_LBRACE)) {
            advance(parser);
            if (!at(parser, TOK_RBRACE)) {
                do {
                    GqlNode *field = gql_node_new(GQL_PROPERTY_TYPE);
                    GqlNode *name = parse_name(parser);
                    if (name) {
                        gql_node_set_text(field, name->text);
                        gql_node_free(name);
                    }
                    match(parser, TOK_COLON);
                    GqlNode *type = parse_value_type(parser);
                    if (type) {
                        gql_node_add_child(field, type);
                    }
                    gql_node_add_child(node, field);
                } while (!parser->failed && match(parser, TOK_COMMA));
            }
            expect(parser, TOK_RBRACE, "}");
        }
        parse_not_null(parser, node);
        return node;
    }
    if (at(parser, TOK_NULL_KW) || at(parser, TOK_NOTHING)) {
        advance(parser);
        node->subkind = GQL_TYPE_NOTHING;
        gql_node_set_text(node, "NULL");
        return node;
    }
    if (at(parser, TOK_NODE) || at(parser, TOK_VERTEX)) {
        advance(parser);
        node->subkind = GQL_TYPE_NODE_REF;
        gql_node_set_text(node, "NODE");
        parse_not_null(parser, node);
        return node;
    }
    if (at(parser, TOK_EDGE) || at(parser, TOK_RELATIONSHIP)) {
        advance(parser);
        node->subkind = GQL_TYPE_EDGE_REF;
        gql_node_set_text(node, "EDGE");
        parse_not_null(parser, node);
        return node;
    }
    if (at(parser, TOK_BINDING) || at(parser, TOK_TABLE)) {
        match(parser, TOK_BINDING);
        advance(parser);
        node->subkind = GQL_TYPE_BINDING_TABLE_REF;
        parse_not_null(parser, node);
        return node;
    }
    if (at(parser, TOK_PROPERTY) || at(parser, TOK_GRAPH)) {
        match(parser, TOK_PROPERTY);
        advance(parser);
        node->subkind = GQL_TYPE_GRAPH_REF;
        parse_not_null(parser, node);
        return node;
    }

    if (is_predefined_type_token(current_token(parser)->kind)) {
        char *s = gql_token_decoded_text(current_token(parser));

        for (char *q = s; *q; q++) {
            if (*q >= 'a' && *q <= 'z') {
                *q = (char)(*q - 'a' + 'A');
            }
        }
        gql_node_take_text(node, s);
        advance(parser);

        if (at(parser, TOK_LPAREN)) {
            advance(parser);
            while (!parser->failed && !at(parser, TOK_RPAREN)) {
                advance(parser);
            }
            expect(parser, TOK_RPAREN, ")");
        }
        parse_not_null(parser, node);
        return node;
    }

    unexpected(parser, "a value type");
    gql_node_free(node);
    return NULL;
}

static void parse_not_null(GqlParser *parser, GqlNode *node) {
    if (at(parser, TOK_NOT)) {
        advance(parser);
        expect(parser, TOK_NULL_KW, "NULL");
        node->integer_value = GQL_NOT_NULL;
    }
}

static GqlNode *parse_yield_clause(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_YIELD);
    expect(parser, TOK_YIELD, "YIELD");
    do {
        GqlNode *item = parse_name(parser);
        if (!item) {
            break;
        }
        if (at(parser, TOK_AS)) {
            advance(parser);
            GqlNode *alias = gql_node_new(GQL_FIELD);
            GqlNode *nm = parse_name(parser);
            if (nm) {
                gql_node_set_text(alias, nm->text);
                gql_node_free(nm);
            }
            gql_node_add_child(alias, item);
            gql_node_add_child(node, alias);
        } else {
            gql_node_add_child(node, item);
        }
    } while (!parser->failed && match(parser, TOK_COMMA));
    return node;
}

static GqlNode *parse_group_by_clause(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_GROUP_BY);
    expect(parser, TOK_GROUP, "GROUP");
    expect(parser, TOK_BY, "BY");
    if (match(parser, TOK_LPAREN)) {
        expect(parser, TOK_RPAREN, ")");
        return node;
    }
    do {
        GqlNode *g = parse_binding_variable_reference(parser);
        if (g) {
            gql_node_add_child(node, g);
        }
    } while (!parser->failed && match(parser, TOK_COMMA));
    return node;
}

static GqlNode *parse_order_by_clause(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_ORDER_BY);
    expect(parser, TOK_ORDER, "ORDER");
    expect(parser, TOK_BY, "BY");
    do {
        GqlNode *spec = gql_node_new(GQL_SORT_SPEC);
        GqlNode *key = parse_value_expression(parser);
        if (!key) {
            break;
        }
        gql_node_add_child(spec, key);
        if (match(parser, TOK_ASC) || match(parser, TOK_ASCENDING)) {
            spec->subkind = GQL_ORDER_ASC;
        } else if (match(parser, TOK_DESC) || match(parser, TOK_DESCENDING)) {
            spec->subkind = GQL_ORDER_DESC;
        }
        if (match(parser, TOK_NULLS)) {
            if (match(parser, TOK_FIRST)) {
                spec->integer_value = GQL_NULL_ORDER_FIRST;
            } else if (match(parser, TOK_LAST)) {
                spec->integer_value = GQL_NULL_ORDER_LAST;
            }
        }
        gql_node_add_child(node, spec);
    } while (!parser->failed && match(parser, TOK_COMMA));
    return node;
}

static GqlNode *parse_limit_clause(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_LIMIT);
    expect(parser, TOK_LIMIT, "LIMIT");
    GqlNode *n = parse_non_negative_integer(parser);
    if (n) {
        gql_node_add_child(node, n);
    }
    return node;
}

static GqlNode *parse_offset_clause(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_OFFSET);
    if (match(parser, TOK_OFFSET)) {
        node->subkind = GQL_OFFSET_OFFSET;
    } else if (match(parser, TOK_SKIP_RESERVED_WORD)) {
        node->subkind = GQL_OFFSET_SKIP;
    }
    GqlNode *n = parse_non_negative_integer(parser);
    if (n) {
        gql_node_add_child(node, n);
    }
    return node;
}

static GqlNode *parse_use_graph_clause(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_USE_GRAPH);
    expect(parser, TOK_USE, "USE");
    GqlNode *e = parse_graph_expression(parser);
    if (e) {
        gql_node_add_child(node, e);
    }
    return node;
}

static GqlNode *parse_non_negative_integer(GqlParser *parser) {
    if (at(parser, TOK_INT_LIT)) {
        return parse_number_literal(parser);
    }
    if (at(parser, TOK_PARAM)) {
        return parse_parameter(parser);
    }
    unexpected(parser, "a non negative integer");
    return NULL;
}

static int binary_operator_precedence(GqlTokenKind k, GqlValueOperation *operation) {
    switch (k) {
    case TOK_OR:
        *operation = GQL_OP_OR;
        return 1;
    case TOK_XOR:
        *operation = GQL_OP_XOR;
        return 1;
    case TOK_AND:
        *operation = GQL_OP_AND;
        return 2;
    case TOK_EQUALS:
        *operation = GQL_OP_EQ;
        return 3;
    case TOK_NE:
        *operation = GQL_OP_NE;
        return 3;
    case TOK_LEFT_ANGLE:
        *operation = GQL_OP_LT;
        return 3;
    case TOK_RIGHT_ANGLE:
        *operation = GQL_OP_GT;
        return 3;
    case TOK_LE:
        *operation = GQL_OP_LE;
        return 3;
    case TOK_GE:
        *operation = GQL_OP_GE;
        return 3;
    case TOK_CONCATENATION:
        *operation = GQL_OP_CONCAT;
        return 4;
    case TOK_PLUS:
        *operation = GQL_OP_ADD;
        return 5;
    case TOK_MINUS:
        *operation = GQL_OP_SUB;
        return 5;
    case TOK_ASTERISK:
        *operation = GQL_OP_MUL;
        return 6;
    case TOK_SOLIDUS:
        *operation = GQL_OP_DIV;
        return 6;
    default:
        return 0;
    }
}

static GqlNode *parse_search_condition(GqlParser *parser) {
    return parse_value_expression(parser);
}

static GqlNode *parse_value_expression(GqlParser *parser) {
    return parse_binary(parser, 1);
}

static GqlNode *parse_binary(GqlParser *parser, int min_prec) {
    GqlNode *lhs = parse_unary(parser);
    if (!lhs) {
        return NULL;
    }
    for (;;) {
        GqlValueOperation operation;
        int precedence = binary_operator_precedence(current_token(parser)->kind, &operation);
        if (precedence < min_prec) {
            break;
        }
        advance(parser);
        GqlNode *rhs = parse_binary(parser, precedence + 1);
        if (!rhs) {
            gql_node_free(lhs);
            return NULL;
        }
        GqlNode *binary = gql_node_new(GQL_VALUE_EXPR);
        binary->subkind = (int)operation;
        gql_node_add_child(binary, lhs);
        gql_node_add_child(binary, rhs);
        lhs = binary;
    }
    return lhs;
}

static GqlNode *parse_unary(GqlParser *parser) {
    if (match(parser, TOK_PLUS)) {
        GqlNode *operand = parse_unary(parser);
        if (!operand) {
            return NULL;
        }
        GqlNode *n = gql_node_new(GQL_VALUE_EXPR);
        n->subkind = GQL_OP_POS;
        gql_node_add_child(n, operand);
        return n;
    }
    if (match(parser, TOK_MINUS)) {
        GqlNode *operand = parse_unary(parser);
        if (!operand) {
            return NULL;
        }
        GqlNode *n = gql_node_new(GQL_VALUE_EXPR);
        n->subkind = GQL_OP_NEG;
        gql_node_add_child(n, operand);
        return n;
    }
    if (match(parser, TOK_NOT)) {
        GqlNode *operand = parse_unary(parser);
        if (!operand) {
            return NULL;
        }
        GqlNode *n = gql_node_new(GQL_VALUE_EXPR);
        n->subkind = GQL_OP_NOT;
        gql_node_add_child(n, operand);
        return n;
    }
    if (at(parser, TOK_EXISTS)) {
        return parse_exists_predicate(parser);
    }
    if (at(parser, TOK_ALL_DIFFERENT)) {
        return parse_all_different_predicate(parser);
    }
    if (at(parser, TOK_SAME)) {
        return parse_same_predicate(parser);
    }
    if (at(parser, TOK_PROPERTY_EXISTS)) {
        return parse_property_exists_predicate(parser);
    }
    if (at(parser, TOK_PROPERTY) && peek(parser, 1)->kind == TOK_GRAPH) {
        advance(parser);
        advance(parser);
        GqlNode *n = gql_node_new(GQL_GRAPH_EXPR);
        GqlNode *e = parse_graph_expression(parser);
        if (e) {
            gql_node_add_child(n, e);
        }
        return n;
    }
    return parse_postfix(parser);
}

static GqlNode *parse_postfix(GqlParser *parser) {
    GqlNode *node = parse_primary(parser);
    if (!node) {
        return NULL;
    }
    for (;;) {
        if (at(parser, TOK_PERIOD) && token_is_name(peek(parser, 1))) {
            advance(parser);
            GqlNode *prop = gql_node_new(GQL_PROPERTY_REFERENCE);
            gql_node_add_child(prop, node);
            GqlNode *name = parse_name(parser);
            if (name) {
                gql_node_add_child(prop, name);
            }
            node = prop;
            continue;
        }
        if (at(parser, TOK_IS)) {
            node = parse_is_predicate(parser, node);
            continue;
        }
        break;
    }
    return node;
}

static GqlNode *parse_is_predicate(GqlParser *parser, GqlNode *operand) {
    advance(parser);
    int neg = match(parser, TOK_NOT);

    if (at(parser, TOK_NULL_KW)) {
        advance(parser);
        GqlNode *n = gql_node_new(GQL_PREDICATE);
        n->subkind = neg ? GQL_PRED_IS_NOT_NULL : GQL_PRED_IS_NULL;
        gql_node_add_child(n, operand);
        return n;
    }
    if (at(parser, TOK_BOOLEAN_LITERAL)) {
        GqlBooleanValue value = GQL_BOOLEAN_TRUE;
        if (ascii_ieq(current_token(parser), "TRUE")) {
            value = GQL_BOOLEAN_TRUE;
        } else if (ascii_ieq(current_token(parser), "FALSE")) {
            value = GQL_BOOLEAN_FALSE;
        } else {
            value = GQL_BOOLEAN_UNKNOWN;
        }
        advance(parser);
        GqlNode *n = gql_node_new(GQL_VALUE_EXPR);
        if (value == GQL_BOOLEAN_TRUE) {
            n->subkind = neg ? GQL_OP_IS_NOT_TRUE : GQL_OP_IS_TRUE;
        } else if (value == GQL_BOOLEAN_FALSE) {
            n->subkind = neg ? GQL_OP_IS_NOT_FALSE : GQL_OP_IS_FALSE;
        } else {
            n->subkind = neg ? GQL_OP_IS_NOT_UNKNOWN : GQL_OP_IS_UNKNOWN;
        }
        gql_node_add_child(n, operand);
        return n;
    }
    if (at(parser, TOK_TYPED) || at(parser, TOK_DOUBLE_COLON)) {
        advance(parser);
        GqlNode *n = gql_node_new(GQL_PREDICATE);
        n->subkind = neg ? GQL_PRED_IS_NOT_TYPED : GQL_PRED_IS_TYPED;
        gql_node_add_child(n, operand);
        GqlNode *t = parse_value_type(parser);
        if (t) {
            gql_node_add_child(n, t);
        }
        return n;
    }
    if (at(parser, TOK_NORMALIZED)) {
        advance(parser);
        match(parser, TOK_NFC);
        match(parser, TOK_NFD);
        match(parser, TOK_NFKC);
        match(parser, TOK_NFKD);
        GqlNode *n = gql_node_new(GQL_VALUE_EXPR);
        n->subkind = neg ? GQL_OP_IS_NOT_NORMALIZED : GQL_OP_IS_NORMALIZED;
        gql_node_add_child(n, operand);
        return n;
    }
    if (at(parser, TOK_DIRECTED)) {
        advance(parser);
        GqlNode *n = gql_node_new(GQL_PREDICATE);
        n->subkind = neg ? GQL_PRED_IS_NOT_DIRECTED : GQL_PRED_IS_DIRECTED;
        gql_node_add_child(n, operand);
        return n;
    }
    if (at(parser, TOK_LABELED)) {
        advance(parser);
        GqlNode *n = gql_node_new(GQL_PREDICATE);
        n->subkind = neg ? GQL_PRED_IS_NOT_LABELED : GQL_PRED_IS_LABELED;
        gql_node_add_child(n, operand);
        GqlNode *label = parse_label_expression(parser);
        if (label) {
            gql_node_add_child(n, label);
        }
        return n;
    }
    if (at(parser, TOK_SOURCE)) {
        advance(parser);
        expect(parser, TOK_OF, "OF");
        GqlNode *n = gql_node_new(GQL_PREDICATE);
        n->subkind = neg ? GQL_PRED_NOT_SOURCE_OF : GQL_PRED_SOURCE_OF;
        gql_node_add_child(n, operand);
        GqlNode *edge = parse_binding_variable_reference(parser);
        if (edge) {
            gql_node_add_child(n, edge);
        }
        return n;
    }
    if (at(parser, TOK_DESTINATION)) {
        advance(parser);
        expect(parser, TOK_OF, "OF");
        GqlNode *n = gql_node_new(GQL_PREDICATE);
        n->subkind = neg ? GQL_PRED_NOT_DESTINATION_OF : GQL_PRED_DESTINATION_OF;
        gql_node_add_child(n, operand);
        GqlNode *edge = parse_binding_variable_reference(parser);
        if (edge) {
            gql_node_add_child(n, edge);
        }
        return n;
    }
    unexpected(parser, "a predicate after IS");
    return operand;
}

static GqlNode *parse_exists_predicate(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_PREDICATE);
    node->subkind = GQL_PRED_EXISTS;
    advance(parser);
    GqlTokenKind open = current_token(parser)->kind;
    if (open != TOK_LBRACE && open != TOK_LPAREN) {
        unexpected(parser, "'{' or '(' after EXISTS");
        gql_node_free(node);
        return NULL;
    }
    GqlTokenKind close = (open == TOK_LBRACE) ? TOK_RBRACE : TOK_RPAREN;
    advance(parser);

    if (at(parser, TOK_MATCH) || at(parser, TOK_OPTIONAL)) {

        GqlNode *block = gql_node_new(GQL_GRAPH_PATTERN);
        while (!parser->failed && !at(parser, close)) {
            GqlNode *s = parse_statement(parser);
            if (!s) {
                break;
            }
            gql_node_add_child(block, s);
        }
        gql_node_add_child(node, block);
    } else {
        GqlNode *g = parse_graph_pattern(parser);
        if (g) {
            gql_node_add_child(node, g);
        }
    }
    expect(parser, close, "a closing '}' or ')'");
    return node;
}

static GqlNode *parse_all_different_predicate(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_PREDICATE);
    node->subkind = GQL_PRED_ALL_DIFFERENT;
    advance(parser);
    expect(parser, TOK_LPAREN, "(");
    do {
        GqlNode *v = parse_binding_variable_reference(parser);
        if (v) {
            gql_node_add_child(node, v);
        }
    } while (!parser->failed && match(parser, TOK_COMMA));
    expect(parser, TOK_RPAREN, ")");
    return node;
}

static GqlNode *parse_same_predicate(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_PREDICATE);
    node->subkind = GQL_PRED_SAME;
    advance(parser);
    expect(parser, TOK_LPAREN, "(");
    do {
        GqlNode *v = parse_binding_variable_reference(parser);
        if (v) {
            gql_node_add_child(node, v);
        }
    } while (!parser->failed && match(parser, TOK_COMMA));
    expect(parser, TOK_RPAREN, ")");
    return node;
}

static GqlNode *parse_property_exists_predicate(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_PREDICATE);
    node->subkind = GQL_PRED_PROPERTY_EXISTS;
    advance(parser);
    expect(parser, TOK_LPAREN, "(");
    GqlNode *v = parse_binding_variable_reference(parser);
    if (v) {
        gql_node_add_child(node, v);
    }
    expect(parser, TOK_COMMA, ",");
    GqlNode *name = parse_name(parser);
    if (name) {
        gql_node_add_child(node, name);
    }
    expect(parser, TOK_RPAREN, ")");
    return node;
}

static GqlNode *parse_binding_variable_reference(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_BINDING_VAR);
    GqlNode *name = parse_name(parser);
    if (name) {
        gql_node_set_text(node, name->text);
        gql_node_free(name);
    }
    return node;
}

static GqlNode *parse_parameter(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_PARAM);
    GqlToken t = *current_token(parser);
    advance(parser);
    char *full = gql_token_decoded_text(&t);
    if (full && full[0] == '$') {
        int is_double = (full[1] == '$');
        gql_node_take_text(node, duplicate_string(full + (is_double ? 2 : 1)));
        node->subkind = is_double ? GQL_PARAMETER_DOUBLE : GQL_PARAMETER_SINGLE;
        free(full);
    } else {
        gql_node_take_text(node, full);
    }
    return node;
}

static GqlNode *parse_property_key_value_pair(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_PROPERTY_SPEC);
    GqlNode *name = parse_name(parser);
    if (name) {
        gql_node_set_text(node, name->text);
        gql_node_free(name);
    }
    expect(parser, TOK_COLON, ":");
    GqlNode *v = parse_value_expression(parser);
    if (v) {
        gql_node_add_child(node, v);
    }
    return node;
}

static GqlNode *parse_primary(GqlParser *parser) {
    GqlToken t = *current_token(parser);

    if (at(parser, TOK_INT_LIT)) {
        return parse_number_literal(parser);
    }
    if (at(parser, TOK_STR_LIT)) {
        advance(parser);
        GqlNode *n = gql_node_new(GQL_LITERAL);
        n->subkind = GQL_LIT_STRING;
        gql_node_take_text(n, gql_token_decoded_text(&t));
        return n;
    }
    if (at(parser, TOK_BYTE_STRING)) {
        advance(parser);
        GqlNode *n = gql_node_new(GQL_LITERAL);
        n->subkind = GQL_LIT_BYTE_STRING;
        gql_node_take_text(n, gql_token_decoded_text(&t));
        return n;
    }
    if (at(parser, TOK_BOOLEAN_LITERAL)) {
        advance(parser);
        GqlNode *n = gql_node_new(GQL_LITERAL);
        n->subkind = GQL_LIT_BOOLEAN;
        if (ascii_ieq(&t, "TRUE")) {
            n->integer_value = GQL_BOOLEAN_TRUE;
        } else if (ascii_ieq(&t, "FALSE")) {
            n->integer_value = GQL_BOOLEAN_FALSE;
        } else {
            n->integer_value = GQL_BOOLEAN_UNKNOWN;
        }
        return n;
    }
    if (at(parser, TOK_NULL_KW)) {
        advance(parser);
        GqlNode *n = gql_node_new(GQL_LITERAL);
        n->subkind = GQL_LIT_NULL;
        return n;
    }
    if (at(parser, TOK_PARAM)) {
        return parse_parameter(parser);
    }

    if ((at(parser, TOK_DATE) || at(parser, TOK_TIME) || at(parser, TOK_DATETIME) ||
         at(parser, TOK_TIMESTAMP) || at(parser, TOK_DURATION)) &&
        peek(parser, 1)->kind == TOK_STR_LIT) {
        GqlLiteralKind lit;
        if (at(parser, TOK_DATE)) {
            lit = GQL_LIT_DATE;
        } else if (at(parser, TOK_TIME)) {
            lit = GQL_LIT_TIME;
        } else if (at(parser, TOK_DATETIME)) {
            lit = GQL_LIT_DATETIME;
        } else if (at(parser, TOK_TIMESTAMP)) {
            lit = GQL_LIT_TIMESTAMP;
        } else {
            lit = GQL_LIT_DURATION;
        }
        advance(parser);
        GqlToken s = *current_token(parser);
        advance(parser);
        GqlNode *n = gql_node_new(GQL_LITERAL);
        n->subkind = (int)lit;
        gql_node_take_text(n, gql_token_decoded_text(&s));
        return n;
    }

    if (at(parser, TOK_LPAREN)) {
        advance(parser);
        GqlNode *n = parse_value_expression(parser);
        expect(parser, TOK_RPAREN, ")");
        return n;
    }
    if (at(parser, TOK_LBRACKET)) {
        return parse_list_literal(parser, NULL);
    }
    if (at(parser, TOK_LBRACE)) {
        return parse_record_literal(parser);
    }
    if (at(parser, TOK_LIST) || at(parser, TOK_ARRAY)) {
        advance(parser);
        if (at(parser, TOK_LBRACKET)) {
            return parse_list_literal(parser, NULL);
        }

        unexpected(parser, "'[' after LIST/ARRAY");
        return NULL;
    }
    if (at(parser, TOK_RECORD)) {
        advance(parser);
        return parse_record_literal(parser);
    }
    if (at(parser, TOK_PATH) && peek(parser, 1)->kind == TOK_LBRACKET) {
        advance(parser);
        GqlNode *node = gql_node_new(GQL_PATH_CONSTRUCTOR);
        advance(parser);
        if (!at(parser, TOK_RBRACKET)) {
            do {
                GqlNode *e = parse_value_expression(parser);
                if (e) {
                    gql_node_add_child(node, e);
                }
            } while (!parser->failed && match(parser, TOK_COMMA));
        }
        expect(parser, TOK_RBRACKET, "]");
        return node;
    }
    if (at(parser, TOK_CASE) || at(parser, TOK_NULLIF) || at(parser, TOK_COALESCE)) {
        return parse_case_expression(parser);
    }
    if (at(parser, TOK_CAST)) {
        return parse_cast_specification(parser);
    }
    if (at(parser, TOK_LET)) {
        advance(parser);
        GqlNode *node = gql_node_new(GQL_LET);
        do {
            GqlNode *def = gql_node_new(GQL_VARIABLE_DECL);
            GqlNode *name = parse_name(parser);
            if (name) {
                gql_node_add_child(def, name);
            }
            if (match(parser, TOK_EQUALS)) {
                GqlNode *v = parse_value_expression(parser);
                if (v) {
                    gql_node_add_child(def, v);
                }
            }
            gql_node_add_child(node, def);
        } while (!parser->failed && match(parser, TOK_COMMA));
        expect(parser, TOK_IN, "IN");
        GqlNode *v = parse_value_expression(parser);
        if (v) {
            gql_node_add_child(node, v);
        }
        expect(parser, TOK_END, "END");
        return node;
    }
    if (at(parser, TOK_VALUE)) {
        advance(parser);
        GqlNode *node = gql_node_new(GQL_VALUE_EXPR);

        expect(parser, TOK_LBRACE, "{");
        while (!parser->failed && !at(parser, TOK_RBRACE)) {
            GqlNode *s = parse_statement(parser);
            if (!s) {
                break;
            }
            gql_node_add_child(node, s);
        }
        expect(parser, TOK_RBRACE, "}");
        return node;
    }

    if (token_is_name(&t) && peek(parser, 1)->kind == TOK_LPAREN) {
        return parse_function_or_aggregate(parser);
    }
    if (token_is_name(&t)) {
        return parse_binding_variable_reference(parser);
    }

    unexpected(parser, "a value expression");
    return NULL;
}

static GqlNode *parse_value_expression_primary(GqlParser *parser) {
    return parse_primary(parser);
}

static GqlNode *parse_number_literal(GqlParser *parser) {
    GqlToken t = *current_token(parser);
    advance(parser);
    GqlNode *n = gql_node_new(GQL_LITERAL);

    size_t nlen = t.length;
    char *clean = malloc(nlen + 1);
    size_t m = 0;
    int is_float = t.is_float;
    for (size_t i = 0; i < nlen; i++) {
        char c = t.start[i];
        if (c == '_' || c == 'M' || c == 'm' || c == 'F' || c == 'f' || c == 'D' || c == 'd') {
            continue;
        }
        clean[m++] = c;
    }
    clean[m] = '\0';

    if (!is_float && (strncmp(clean, "0x", 2) == 0 || strncmp(clean, "0X", 2) == 0)) {
        n->subkind = GQL_LIT_INTEGER;
        n->unsigned_integer_value = strtoull(clean + 2, NULL, 16);
    } else if (!is_float && (strncmp(clean, "0o", 2) == 0 || strncmp(clean, "0O", 2) == 0)) {
        n->subkind = GQL_LIT_INTEGER;
        n->unsigned_integer_value = strtoull(clean + 2, NULL, 8);
    } else if (!is_float && (strncmp(clean, "0b", 2) == 0 || strncmp(clean, "0B", 2) == 0)) {
        n->subkind = GQL_LIT_INTEGER;
        n->unsigned_integer_value = strtoull(clean + 2, NULL, 2);
    } else if (is_float) {
        n->subkind = GQL_LIT_FLOAT;
        n->floating_value = strtod(clean, NULL);
    } else {
        n->subkind = GQL_LIT_INTEGER;
        n->unsigned_integer_value = strtoull(clean, NULL, 10);
    }
    free(clean);
    return n;
}

static GqlNode *parse_list_literal(GqlParser *parser, const char *type_name) {
    GqlNode *node = gql_node_new(GQL_LIST_LITERAL);
    if (type_name) {
        gql_node_set_text(node, type_name);
    }
    expect(parser, TOK_LBRACKET, "[");
    if (!at(parser, TOK_RBRACKET)) {
        do {
            GqlNode *e = parse_value_expression(parser);
            if (e) {
                gql_node_add_child(node, e);
            }
        } while (!parser->failed && match(parser, TOK_COMMA));
    }
    expect(parser, TOK_RBRACKET, "]");
    return node;
}

static GqlNode *parse_record_literal(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_RECORD_LITERAL);
    expect(parser, TOK_LBRACE, "{");
    if (!at(parser, TOK_RBRACE)) {
        do {
            GqlNode *field = gql_node_new(GQL_FIELD);
            GqlNode *name = parse_name(parser);
            if (name) {
                gql_node_set_text(field, name->text);
                gql_node_free(name);
            }
            expect(parser, TOK_COLON, ":");
            GqlNode *v = parse_value_expression(parser);
            if (v) {
                gql_node_add_child(field, v);
            }
            gql_node_add_child(node, field);
        } while (!parser->failed && match(parser, TOK_COMMA));
    }
    expect(parser, TOK_RBRACE, "}");
    return node;
}

static GqlNode *parse_case_expression(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_CASE);
    if (at(parser, TOK_NULLIF)) {
        advance(parser);
        expect(parser, TOK_LPAREN, "(");
        GqlNode *a = parse_value_expression(parser);
        if (a) {
            gql_node_add_child(node, a);
        }
        expect(parser, TOK_COMMA, ",");
        GqlNode *b = parse_value_expression(parser);
        if (b) {
            gql_node_add_child(node, b);
        }
        expect(parser, TOK_RPAREN, ")");
        node->subkind = GQL_CASE_NULLIF;
        return node;
    }
    if (at(parser, TOK_COALESCE)) {
        advance(parser);
        expect(parser, TOK_LPAREN, "(");
        do {
            GqlNode *a = parse_value_expression(parser);
            if (a) {
                gql_node_add_child(node, a);
            }
        } while (!parser->failed && match(parser, TOK_COMMA));
        expect(parser, TOK_RPAREN, ")");
        node->subkind = GQL_CASE_COALESCE;
        return node;
    }

    expect(parser, TOK_CASE, "CASE");
    if (!at(parser, TOK_WHEN)) {
        GqlNode *operand = parse_value_expression(parser);
        if (operand) {
            gql_node_add_child(node, operand);
        }
    }
    while (!parser->failed && at(parser, TOK_WHEN)) {
        advance(parser);
        GqlNode *when = gql_node_new(GQL_VALUE_EXPR);
        GqlNode *cond = parse_value_expression(parser);
        if (cond) {
            gql_node_add_child(when, cond);
        }
        expect(parser, TOK_THEN, "THEN");
        GqlNode *res = parse_value_expression(parser);
        if (res) {
            gql_node_add_child(when, res);
        }
        gql_node_add_child(node, when);
    }
    if (at(parser, TOK_ELSE)) {
        advance(parser);
        GqlNode *res = parse_value_expression(parser);
        if (res) {
            gql_node_add_child(node, res);
        }
    }
    expect(parser, TOK_END, "END");
    return node;
}

static GqlNode *parse_cast_specification(GqlParser *parser) {
    GqlNode *node = gql_node_new(GQL_CAST);
    expect(parser, TOK_CAST, "CAST");
    expect(parser, TOK_LPAREN, "(");
    GqlNode *operand = parse_value_expression(parser);
    if (operand) {
        gql_node_add_child(node, operand);
    }
    expect(parser, TOK_AS, "AS");
    GqlNode *type = parse_value_type(parser);
    if (type) {
        gql_node_add_child(node, type);
    }
    expect(parser, TOK_RPAREN, ")");
    return node;
}

static int lookup_aggregate(const char *name, GqlAggregateKind *out) {
    static const struct {
        const char *n;
        GqlAggregateKind k;
    } tab[] = {{"COUNT", GQL_AGG_COUNT},
               {"AVG", GQL_AGG_AVG},
               {"MAX", GQL_AGG_MAX},
               {"MIN", GQL_AGG_MIN},
               {"SUM", GQL_AGG_SUM},
               {"COLLECT_LIST", GQL_AGG_COLLECT_LIST},
               {"STDDEV_SAMP", GQL_AGG_STDDEV_SAMP},
               {"STDDEV_POP", GQL_AGG_STDDEV_POP},
               {"PERCENTILE_CONT", GQL_AGG_PERCENTILE_CONT},
               {"PERCENTILE_DISC", GQL_AGG_PERCENTILE_DISC}};
    for (size_t i = 0; i < sizeof(tab) / sizeof(tab[0]); i++) {
        if (ieq(tab[i].n, name)) {
            *out = tab[i].k;
            return 1;
        }
    }
    return 0;
}

static int lookup_function(const char *name, GqlFunctionKind *out) {
    static const struct {
        const char *n;
        GqlFunctionKind k;
    } tab[] = {{"ABS", GQL_FN_ABS},
               {"UPPER", GQL_FN_UPPER},
               {"LOWER", GQL_FN_LOWER},
               {"TRIM", GQL_FN_TRIM},
               {"BTRIM", GQL_FN_BTRIM},
               {"LTRIM", GQL_FN_LTRIM},
               {"RTRIM", GQL_FN_RTRIM},
               {"NORMALIZE", GQL_FN_NORMALIZE},
               {"LEFT", GQL_FN_LEFT},
               {"RIGHT", GQL_FN_RIGHT},
               {"CHAR_LENGTH", GQL_FN_CHAR_LENGTH},
               {"CHARACTER_LENGTH", GQL_FN_CHARACTER_LENGTH},
               {"BYTE_LENGTH", GQL_FN_BYTE_LENGTH},
               {"OCTET_LENGTH", GQL_FN_OCTET_LENGTH},
               {"PATH_LENGTH", GQL_FN_PATH_LENGTH},
               {"CARDINALITY", GQL_FN_CARDINALITY},
               {"SIZE", GQL_FN_SIZE},
               {"MOD", GQL_FN_MOD},
               {"SIN", GQL_FN_SIN},
               {"COS", GQL_FN_COS},
               {"TAN", GQL_FN_TAN},
               {"COT", GQL_FN_COT},
               {"SINH", GQL_FN_SINH},
               {"COSH", GQL_FN_COSH},
               {"TANH", GQL_FN_TANH},
               {"ASIN", GQL_FN_ASIN},
               {"ACOS", GQL_FN_ACOS},
               {"ATAN", GQL_FN_ATAN},
               {"DEGREES", GQL_FN_DEGREES},
               {"RADIANS", GQL_FN_RADIANS},
               {"LOG", GQL_FN_LOG},
               {"LOG10", GQL_FN_LOG10},
               {"LN", GQL_FN_LN},
               {"EXP", GQL_FN_EXP},
               {"POWER", GQL_FN_POWER},
               {"SQRT", GQL_FN_SQRT},
               {"FLOOR", GQL_FN_FLOOR},
               {"CEIL", GQL_FN_CEIL},
               {"CEILING", GQL_FN_CEILING},
               {"ELEMENTS", GQL_FN_ELEMENTS},
               {"ELEMENT_ID", GQL_FN_ELEMENT_ID},
               {"DATE", GQL_FN_DATE},
               {"TIME", GQL_FN_TIME},
               {"LOCAL_TIME", GQL_FN_LOCAL_TIME},
               {"DATETIME", GQL_FN_DATETIME},
               {"LOCAL_DATETIME", GQL_FN_LOCAL_DATETIME},
               {"DURATION", GQL_FN_DURATION},
               {"DURATION_BETWEEN", GQL_FN_DURATION_BETWEEN},
               {"CURRENT_DATE", GQL_FN_CURRENT_DATE},
               {"CURRENT_TIME", GQL_FN_CURRENT_TIME},
               {"CURRENT_TIMESTAMP", GQL_FN_CURRENT_TIMESTAMP},
               {"LOCAL_TIMESTAMP", GQL_FN_LOCAL_TIMESTAMP},
               {"ZONED_TIME", GQL_FN_ZONED_TIME},
               {"ZONED_DATETIME", GQL_FN_ZONED_DATETIME}};
    for (size_t i = 0; i < sizeof(tab) / sizeof(tab[0]); i++) {
        if (ieq(tab[i].n, name)) {
            *out = tab[i].k;
            return 1;
        }
    }
    return 0;
}

static GqlNode *parse_function_or_aggregate(GqlParser *parser) {
    const GqlToken *t = current_token(parser);
    char *name = gql_token_decoded_text(t);
    GqlAggregateKind agg;
    GqlFunctionKind fn;

    if (lookup_aggregate(name, &agg)) {
        free(name);
        return parse_aggregate(parser, agg);
    }
    if (lookup_function(name, &fn)) {
        free(name);
        return parse_function(parser, fn);
    }
    free(name);
    unexpected(parser, "a known function");
    return NULL;
}

static GqlNode *parse_aggregate(GqlParser *parser, GqlAggregateKind agg) {
    GqlNode *node = gql_node_new(GQL_AGGREGATE);
    node->subkind = (int)agg;
    advance(parser);

    if (agg == GQL_AGG_COUNT && peek(parser, 1)->kind == TOK_ASTERISK) {
        advance(parser);
        advance(parser);
        expect(parser, TOK_RPAREN, ")");
        return node;
    }
    expect(parser, TOK_LPAREN, "(");
    if (match(parser, TOK_DISTINCT)) {
        node->integer_value = GQL_SQ_DISTINCT;
    } else if (match(parser, TOK_ALL)) {
        node->integer_value = GQL_SQ_ALL;
    }
    if (agg == GQL_AGG_PERCENTILE_CONT || agg == GQL_AGG_PERCENTILE_DISC) {
        GqlNode *a = parse_value_expression(parser);
        if (a) {
            gql_node_add_child(node, a);
        }
        expect(parser, TOK_COMMA, ",");
        GqlNode *b = parse_value_expression(parser);
        if (b) {
            gql_node_add_child(node, b);
        }
    } else {
        GqlNode *a = parse_value_expression(parser);
        if (a) {
            gql_node_add_child(node, a);
        }
    }
    expect(parser, TOK_RPAREN, ")");
    return node;
}

static GqlNode *parse_function(GqlParser *parser, GqlFunctionKind fn) {
    GqlNode *node = gql_node_new(GQL_FUNCTION_CALL);
    node->subkind = (int)fn;
    advance(parser);
    expect(parser, TOK_LPAREN, "(");
    if (fn == GQL_FN_CURRENT_DATE || fn == GQL_FN_CURRENT_TIME || fn == GQL_FN_CURRENT_TIMESTAMP ||
        fn == GQL_FN_LOCAL_TIMESTAMP) {
        expect(parser, TOK_RPAREN, ")");
        return node;
    }
    if (!at(parser, TOK_RPAREN)) {
        do {
            GqlNode *a = parse_value_expression(parser);
            if (a) {
                gql_node_add_child(node, a);
            }
        } while (!parser->failed && match(parser, TOK_COMMA));
    }
    expect(parser, TOK_RPAREN, ")");
    return node;
}

static char *duplicate_string(const char *s) {
    if (!s) {
        return NULL;
    }
    size_t n = strlen(s);
    char *out = malloc(n + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, s, n + 1);
    return out;
}

static int ieq(const char *a, const char *b) {
    while (*a && *b) {
        int ca = (*a >= 'a' && *a <= 'z') ? *a - 'a' + 'A' : *a;
        int cb = (*b >= 'a' && *b <= 'z') ? *b - 'a' + 'A' : *b;
        if (ca != cb) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int ascii_ieq(const GqlToken *t, const char *s) {
    size_t n = strlen(s);
    if (t->length != n) {
        return 0;
    }
    for (size_t i = 0; i < n; i++) {
        int ca = (t->start[i] >= 'a' && t->start[i] <= 'z') ? t->start[i] - 'a' + 'A' : t->start[i];
        if (ca != s[i]) {
            return 0;
        }
    }
    return 1;
}

static char *slice(GqlToken first, GqlToken last) {
    if (!first.start || !last.start || last.start < first.start) {
        return duplicate_string("");
    }
    size_t n = (size_t)((last.start + last.length) - first.start);
    char *out = malloc(n + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, first.start, n);
    out[n] = '\0';
    return out;
}

static void fill_error(const GqlParser *parser, GqlParseError *error) {
    if (!error) {
        return;
    }
    *error = parser->error;
}

GqlNode *gql_parse_program(const char *source, GqlParseError *error) {
    GqlParser parser;
    gql_parser_init(&parser, source);
    if (error) {
        *error = (GqlParseError){0};
    }

    GqlNode *prog = parse_program(&parser);
    if (!parser.failed && !at(&parser, TOK_EOF)) {
        unexpected(&parser, "end of input");
    }
    if (parser.failed) {
        gql_node_free(prog);
        fill_error(&parser, error);
        return NULL;
    }
    if (error) {
        error->code = GQL_ERR_NONE;
    }
    return prog;
}

GqlNode *gql_parse_script(const char *source, GqlParseError *error) {
    GqlParser parser;
    gql_parser_init(&parser, source);
    if (error) {
        *error = (GqlParseError){0};
    }

    GqlNode *all = gql_node_new(GQL_PROGRAM);
    while (!parser.failed && !at(&parser, TOK_EOF)) {
        GqlNode *prog = parse_program(&parser);
        if (!prog) {
            break;
        }
        for (size_t i = 0; i < prog->child_count; i++) {
            gql_node_add_child(all, prog->children[i]);
        }
        free(prog->children);
        prog->children = NULL;
        prog->child_count = 0;
        gql_node_free(prog);
    }
    if (parser.failed) {
        gql_node_free(all);
        fill_error(&parser, error);
        return NULL;
    }
    if (error) {
        error->code = GQL_ERR_NONE;
    }
    return all;
}

const char *gql_error_string(int code) {
    switch (code) {
    case GQL_ERR_NONE:
        return "no error";
    case GQL_ERR_LEX:
        return "lexical error";
    case GQL_ERR_UNEXPECTED_TOKEN:
        return "unexpected token";
    case GQL_ERR_UNEXPECTED_EOF:
        return "unexpected end of input";
    case GQL_ERR_OOM:
        return "out of memory";
    case GQL_ERR_SYNTAX:
        return "syntax error";
    default:
        return "unknown error";
    }
}
