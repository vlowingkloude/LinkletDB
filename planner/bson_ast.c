#include "bson_ast.h"

#include "bson_writer.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void set_error(char *error, const size_t error_size, const char *format, ...) {
    if (!error || error_size == 0) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(error, error_size, format, args);
    va_end(args);
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

bool linklet_bson_append_gql_literal(LinkletBson *document, const char *key, const GqlNode *literal,
                                     char *error, const size_t error_size) {
    if (!literal || literal->kind != GQL_LITERAL) {
        set_error(error, error_size, "only literal property values are supported");
        return false;
    }
    LinkletBsonError bson_error;
    bool success = false;
    switch ((GqlLiteralKind)literal->subkind) {
    case GQL_LIT_INTEGER:
        if (literal->unsigned_integer_value <= INT32_MAX) {
            success = linklet_bson_append_int32(
                document, key, (int32_t)literal->unsigned_integer_value, &bson_error);
        } else if (literal->unsigned_integer_value <= INT64_MAX) {
            success = linklet_bson_append_int64(
                document, key, (int64_t)literal->unsigned_integer_value, &bson_error);
        } else {
            set_error(error, error_size, "integer property exceeds signed BSON int64");
            return false;
        }
        break;
    case GQL_LIT_FLOAT:
        success = linklet_bson_append_double(document, key, literal->floating_value, &bson_error);
        break;
    case GQL_LIT_STRING:
        success = linklet_bson_append_utf8(document, key, literal->text ? literal->text : "", -1,
                                           &bson_error);
        break;
    case GQL_LIT_BOOLEAN:
        if (literal->integer_value == GQL_BOOLEAN_TRUE ||
            literal->integer_value == GQL_BOOLEAN_FALSE) {
            success = linklet_bson_append_bool(
                document, key, literal->integer_value == GQL_BOOLEAN_TRUE, &bson_error);
        } else {
            success = linklet_bson_append_null(document, key, &bson_error);
        }
        break;
    case GQL_LIT_NULL:
        success = linklet_bson_append_null(document, key, &bson_error);
        break;
    default:
        set_error(error, error_size, "literal type is not supported by the BSON planner");
        return false;
    }
    if (!success) {
        set_error(error, error_size, "%s", bson_error.message);
    }
    return success;
}

static bool append_labels(LinkletBson *array, const GqlNode *label, char *error,
                          const size_t error_size) {
    if (!label) {
        return true;
    }
    if (label->kind == GQL_NAME && label->text) {
        LinkletBsonError bson_error;
        if (!linklet_bson_array_append_utf8(array, label->text, -1, &bson_error)) {
            set_error(error, error_size, "%s", bson_error.message);
            return false;
        }
        return true;
    }
    if (label->kind != GQL_LABEL_EXPRESSION ||
        (label->subkind != GQL_LABEL_CONJUNCTION && label->subkind != GQL_LABEL_NAME)) {
        set_error(error, error_size, "element labels must be a simple label set");
        return false;
    }
    for (size_t child = 0; child < label->child_count; ++child) {
        if (!append_labels(array, label->children[child], error, error_size)) {
            return false;
        }
    }
    return true;
}

bool linklet_encode_element_bson(const GqlNode *element, LinkletBson *document, char *error,
                                 const size_t error_size) {
    const GqlNode *filler = direct_child(element, GQL_ELEMENT_FILLER);
    if (!filler) {
        set_error(error, error_size, "graph element requires an element filler");
        return false;
    }
    linklet_bson_init(document);
    LinkletBson labels;
    linklet_bson_init(&labels);
    const GqlNode *label = direct_child(filler, GQL_LABEL_EXPRESSION);
    LinkletBsonError bson_error;
    bool success = append_labels(&labels, label, error, error_size) &&
                   linklet_bson_append_array(document, "_labels", &labels, &bson_error);
    linklet_bson_destroy(&labels);
    if (!success) {
        if (error && !error[0]) {
            set_error(error, error_size, "%s", bson_error.message);
        }
        linklet_bson_destroy(document);
        return false;
    }
    for (size_t child = 0; child < filler->child_count; ++child) {
        const GqlNode *property = filler->children[child];
        if (property->kind != GQL_PROPERTY_SPEC) {
            continue;
        }
        if (!property->text || strcmp(property->text, "_labels") == 0 ||
            strcmp(property->text, "id") == 0 || strcmp(property->text, "_id") == 0 ||
            property->child_count != 1 ||
            !linklet_bson_append_gql_literal(document, property->text, property->children[0], error,
                                             error_size)) {
            if (error && !error[0]) {
                set_error(error, error_size, "invalid graph element property");
            }
            linklet_bson_destroy(document);
            return false;
        }
    }
    return true;
}
