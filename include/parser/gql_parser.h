#ifndef LINKLET_GQL_PARSER_H
#define LINKLET_GQL_PARSER_H

#include "gql_ast.h"

enum {
    GQL_PARSE_ERROR_MESSAGE_CAPACITY = 256,
};

typedef struct GqlParseError {
    char message[GQL_PARSE_ERROR_MESSAGE_CAPACITY];
    size_t line;
    size_t column;
    int code;
} GqlParseError;

enum {
    GQL_ERR_NONE = 0,
    GQL_ERR_LEX,
    GQL_ERR_UNEXPECTED_TOKEN,
    GQL_ERR_UNEXPECTED_EOF,
    GQL_ERR_OOM,
    GQL_ERR_SYNTAX
};

GqlNode *gql_parse_program(const char *source, GqlParseError *error);
GqlNode *gql_parse_script(const char *source, GqlParseError *error);

const char *gql_error_string(int code);

#endif
