#include <stdio.h>
#include <stdlib.h>

#include "gql_parser.h"

enum {
    GQL_DUMP_PROGRAM_ARGUMENT_INDEX = 0,
    GQL_DUMP_STATEMENT_ARGUMENT_INDEX = 1,
    GQL_DUMP_ARGUMENT_COUNT = 2,
    GQL_DUMP_USAGE_EXIT_CODE = 2,
};

int main(const int argc, char **argv) {
    if (argc != GQL_DUMP_ARGUMENT_COUNT) {
        fprintf(stderr, "usage: %s '<GQL statement>'\n", argv[GQL_DUMP_PROGRAM_ARGUMENT_INDEX]);
        return GQL_DUMP_USAGE_EXIT_CODE;
    }

    GqlParseError error;
    GqlNode *ast = gql_parse_program(argv[GQL_DUMP_STATEMENT_ARGUMENT_INDEX], &error);
    if (!ast) {
        fprintf(stderr, "parse error at %zu:%zu: %s\n", error.line, error.column, error.message);
        return EXIT_FAILURE;
    }
    gql_ast_dump(ast, stdout);
    gql_node_free(ast);
    return EXIT_SUCCESS;
}
