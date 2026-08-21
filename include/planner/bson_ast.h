#ifndef LINKLET_BSON_AST_H
#define LINKLET_BSON_AST_H

#include <stdbool.h>
#include <stddef.h>

#include "bson.h"
#include "gql_ast.h"

bool linklet_bson_append_gql_literal(LinkletBson *document, const char *key, const GqlNode *literal,
                                     char *error, size_t error_size);

bool linklet_encode_element_bson(const GqlNode *element, LinkletBson *document, char *error,
                                 size_t error_size);

#endif
