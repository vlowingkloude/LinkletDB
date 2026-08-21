#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bson_writer.h"
#include "bson_internal.h"
#include "bson_reader.h"

enum {
    LINKLET_BSON_INITIAL_CAPACITY = 64,
    LINKLET_BSON_CAPACITY_GROWTH_FACTOR = 2,
    LINKLET_BSON_ARRAY_KEY_CAPACITY = 24,
};

static bool linklet_bson_reserve_to(LinkletBson *bson, size_t required, LinkletBsonError *error) {
    if (bson->flags & LINKLET_BSON_FLAG_STATIC) {
        linklet_bson_set_error(error, "cannot append to a read-only (static) document");
        return false;
    }
    if (required > (size_t)INT32_MAX) {
        linklet_bson_set_error(error, "document exceeds the maximum BSON size");
        return false;
    }
    if (required <= bson->capacity) {
        return true;
    }

    size_t capacity = bson->capacity ? bson->capacity : LINKLET_BSON_INITIAL_CAPACITY;
    while (capacity < required) {
        if (capacity > SIZE_MAX / LINKLET_BSON_CAPACITY_GROWTH_FACTOR) {
            linklet_bson_set_error(error, "document is too large to allocate");
            return false;
        }
        capacity *= LINKLET_BSON_CAPACITY_GROWTH_FACTOR;
    }
    uint8_t *grown = (uint8_t *)realloc(bson->data, capacity);
    if (!grown) {
        linklet_bson_set_error(error, "out of memory while growing document");
        return false;
    }
    bson->data = grown;
    bson->capacity = capacity;
    return true;
}

static uint8_t *linklet_bson_append_value_begin(LinkletBson *bson, const char *key,
                                                LinkletBsonType type, size_t value_len,
                                                LinkletBsonError *error) {
    if (!bson) {
        linklet_bson_set_error(error, "document is NULL");
        return NULL;
    }
    if (!key || !key[0]) {
        linklet_bson_set_error(error, "element key must not be empty");
        return NULL;
    }
    const size_t key_length = strlen(key);
    if (!linklet_bson_utf8_validate((const uint8_t *)key, key_length)) {
        linklet_bson_set_error(error, "element key '%s' is not valid UTF-8", key);
        return NULL;
    }

    const size_t type_pos = (bson->length == 0) ? 4 : (bson->length - 1);
    if (key_length > (size_t)INT32_MAX || value_len > (size_t)INT32_MAX ||
        type_pos > (size_t)INT32_MAX) {
        linklet_bson_set_error(error, "element is too large");
        return NULL;
    }
    const size_t new_len = type_pos + 1 + key_length + 1 + value_len + 1;
    if (new_len > (size_t)INT32_MAX) {
        linklet_bson_set_error(error, "document exceeds the maximum BSON size");
        return NULL;
    }
    if (!linklet_bson_reserve_to(bson, new_len, error)) {
        return NULL;
    }

    uint8_t *type_p = bson->data + type_pos;
    *type_p = (uint8_t)type;
    uint8_t *key_p = type_p + 1;
    memcpy(key_p, key, key_length + 1);
    uint8_t *value_p = key_p + key_length + 1;
    value_p[value_len] = 0x00;

    bson->length = new_len;
    linklet_bson_store_u32le(bson->data, (uint32_t)new_len);
    return value_p;
}

bool linklet_bson_append_double(LinkletBson *bson, const char *key, double value,
                                LinkletBsonError *error) {
    uint8_t *p = linklet_bson_append_value_begin(bson, key, LINKLET_BSON_TYPE_DOUBLE, 8, error);
    if (!p) {
        return false;
    }
    linklet_bson_store_double_le(p, value);
    return true;
}

bool linklet_bson_append_utf8(LinkletBson *bson, const char *key, const char *value, int length,
                              LinkletBsonError *error) {
    if (!value) {
        linklet_bson_set_error(error, "string value is NULL");
        return false;
    }
    const size_t vlen = (length < 0) ? strlen(value) : (size_t)length;
    if (vlen > (size_t)INT32_MAX - 1) {
        linklet_bson_set_error(error, "string is too long");
        return false;
    }
    if (!linklet_bson_utf8_validate((const uint8_t *)value, vlen)) {
        linklet_bson_set_error(error, "string is not valid UTF-8");
        return false;
    }
    uint8_t *p =
        linklet_bson_append_value_begin(bson, key, LINKLET_BSON_TYPE_UTF8, 4 + vlen + 1, error);
    if (!p) {
        return false;
    }
    linklet_bson_store_u32le(p, (uint32_t)(vlen + 1));
    memcpy(p + 4, value, vlen);
    p[4 + vlen] = 0x00;
    return true;
}

bool linklet_bson_append_document(LinkletBson *bson, const char *key, const LinkletBson *value,
                                  LinkletBsonError *error) {
    if (!value) {
        linklet_bson_set_error(error, "document value is NULL");
        return false;
    }
    const uint8_t *data = linklet_bson_get_data(value);
    const size_t length = linklet_bson_get_length(value);
    uint8_t *p =
        linklet_bson_append_value_begin(bson, key, LINKLET_BSON_TYPE_DOCUMENT, length, error);
    if (!p) {
        return false;
    }
    memcpy(p, data, length);
    return true;
}

bool linklet_bson_append_array(LinkletBson *bson, const char *key, const LinkletBson *value,
                               LinkletBsonError *error) {
    if (!value) {
        linklet_bson_set_error(error, "array value is NULL");
        return false;
    }
    const uint8_t *data = linklet_bson_get_data(value);
    const size_t length = linklet_bson_get_length(value);
    uint8_t *p = linklet_bson_append_value_begin(bson, key, LINKLET_BSON_TYPE_ARRAY, length, error);
    if (!p) {
        return false;
    }
    memcpy(p, data, length);
    return true;
}

bool linklet_bson_append_binary(LinkletBson *bson, const char *key, LinkletBsonSubtype subtype,
                                const uint8_t *data, size_t length, LinkletBsonError *error) {
    if (length > (size_t)INT32_MAX) {
        linklet_bson_set_error(error, "binary data is too long");
        return false;
    }
    if (length != 0 && !data) {
        linklet_bson_set_error(error, "binary data pointer is NULL");
        return false;
    }
    uint8_t *p =
        linklet_bson_append_value_begin(bson, key, LINKLET_BSON_TYPE_BINARY, 5 + length, error);
    if (!p) {
        return false;
    }
    linklet_bson_store_u32le(p, (uint32_t)length);
    p[4] = (uint8_t)subtype;
    if (length != 0) {
        memcpy(p + 5, data, length);
    }
    return true;
}

bool linklet_bson_append_oid(LinkletBson *bson, const char *key, const LinkletBsonOid *oid,
                             LinkletBsonError *error) {
    if (!oid) {
        linklet_bson_set_error(error, "objectId value is NULL");
        return false;
    }
    uint8_t *p = linklet_bson_append_value_begin(bson, key, LINKLET_BSON_TYPE_OID,
                                                 LINKLET_BSON_OID_BYTE_LENGTH, error);
    if (!p) {
        return false;
    }
    memcpy(p, oid->bytes, sizeof(oid->bytes));
    return true;
}

bool linklet_bson_append_bool(LinkletBson *bson, const char *key, bool value,
                              LinkletBsonError *error) {
    uint8_t *p = linklet_bson_append_value_begin(bson, key, LINKLET_BSON_TYPE_BOOL, 1, error);
    if (!p) {
        return false;
    }
    p[0] = value ? 0x01 : 0x00;
    return true;
}

bool linklet_bson_append_date_time(LinkletBson *bson, const char *key, int64_t milliseconds,
                                   LinkletBsonError *error) {
    uint8_t *p = linklet_bson_append_value_begin(bson, key, LINKLET_BSON_TYPE_DATE_TIME, 8, error);
    if (!p) {
        return false;
    }
    linklet_bson_store_u64le(p, (uint64_t)milliseconds);
    return true;
}

bool linklet_bson_append_null(LinkletBson *bson, const char *key, LinkletBsonError *error) {
    return linklet_bson_append_value_begin(bson, key, LINKLET_BSON_TYPE_NULL, 0, error) != NULL;
}

bool linklet_bson_append_undefined(LinkletBson *bson, const char *key, LinkletBsonError *error) {
    return linklet_bson_append_value_begin(bson, key, LINKLET_BSON_TYPE_UNDEFINED, 0, error) !=
           NULL;
}

bool linklet_bson_append_regex(LinkletBson *bson, const char *key, const char *pattern,
                               const char *options, LinkletBsonError *error) {
    if (!pattern) {
        pattern = "";
    }
    if (!options) {
        options = "";
    }
    const size_t plen = strlen(pattern);
    const size_t olen = strlen(options);
    uint8_t *p = linklet_bson_append_value_begin(bson, key, LINKLET_BSON_TYPE_REGEX,
                                                 plen + 1 + olen + 1, error);
    if (!p) {
        return false;
    }
    memcpy(p, pattern, plen + 1);
    memcpy(p + plen + 1, options, olen + 1);
    return true;
}

bool linklet_bson_append_dbpointer(LinkletBson *bson, const char *key, const char *collection,
                                   const LinkletBsonOid *oid, LinkletBsonError *error) {
    if (!collection || !oid) {
        linklet_bson_set_error(error, "dbpointer collection and objectId are required");
        return false;
    }
    const size_t clen = strlen(collection);
    uint8_t *p = linklet_bson_append_value_begin(
        bson, key, LINKLET_BSON_TYPE_DBPOINTER, 4 + clen + 1 + LINKLET_BSON_OID_BYTE_LENGTH, error);
    if (!p) {
        return false;
    }
    linklet_bson_store_u32le(p, (uint32_t)(clen + 1));
    memcpy(p + 4, collection, clen);
    p[4 + clen] = 0x00;
    memcpy(p + 4 + clen + 1, oid->bytes, sizeof(oid->bytes));
    return true;
}

bool linklet_bson_append_code(LinkletBson *bson, const char *key, const char *javascript,
                              LinkletBsonError *error) {
    if (!javascript) {
        linklet_bson_set_error(error, "code value is NULL");
        return false;
    }
    const size_t jlen = strlen(javascript);
    uint8_t *p =
        linklet_bson_append_value_begin(bson, key, LINKLET_BSON_TYPE_CODE, 4 + jlen + 1, error);
    if (!p) {
        return false;
    }
    linklet_bson_store_u32le(p, (uint32_t)(jlen + 1));
    memcpy(p + 4, javascript, jlen);
    p[4 + jlen] = 0x00;
    return true;
}

bool linklet_bson_append_symbol(LinkletBson *bson, const char *key, const char *value,
                                LinkletBsonError *error) {
    if (!value) {
        linklet_bson_set_error(error, "symbol value is NULL");
        return false;
    }
    const size_t vlen = strlen(value);
    uint8_t *p =
        linklet_bson_append_value_begin(bson, key, LINKLET_BSON_TYPE_SYMBOL, 4 + vlen + 1, error);
    if (!p) {
        return false;
    }
    linklet_bson_store_u32le(p, (uint32_t)(vlen + 1));
    memcpy(p + 4, value, vlen);
    p[4 + vlen] = 0x00;
    return true;
}

bool linklet_bson_append_code_with_scope(LinkletBson *bson, const char *key, const char *javascript,
                                         const LinkletBson *scope, LinkletBsonError *error) {
    if (!javascript || !scope) {
        linklet_bson_set_error(error, "code and scope are required");
        return false;
    }
    const size_t jlen = strlen(javascript);
    const uint8_t *scope_data = linklet_bson_get_data(scope);
    const size_t scope_length = linklet_bson_get_length(scope);
    const size_t total = 4 + (4 + jlen + 1) + scope_length;
    if (total > (size_t)INT32_MAX) {
        linklet_bson_set_error(error, "code-with-scope is too large");
        return false;
    }
    uint8_t *p =
        linklet_bson_append_value_begin(bson, key, LINKLET_BSON_TYPE_CODEWSCOPE, total, error);
    if (!p) {
        return false;
    }
    linklet_bson_store_u32le(p, (uint32_t)total);
    linklet_bson_store_u32le(p + 4, (uint32_t)(jlen + 1));
    memcpy(p + 8, javascript, jlen);
    p[8 + jlen] = 0x00;
    memcpy(p + 8 + jlen + 1, scope_data, scope_length);
    return true;
}

bool linklet_bson_append_int32(LinkletBson *bson, const char *key, int32_t value,
                               LinkletBsonError *error) {
    uint8_t *p = linklet_bson_append_value_begin(bson, key, LINKLET_BSON_TYPE_INT32, 4, error);
    if (!p) {
        return false;
    }
    linklet_bson_store_u32le(p, (uint32_t)value);
    return true;
}

bool linklet_bson_append_int64(LinkletBson *bson, const char *key, int64_t value,
                               LinkletBsonError *error) {
    uint8_t *p = linklet_bson_append_value_begin(bson, key, LINKLET_BSON_TYPE_INT64, 8, error);
    if (!p) {
        return false;
    }
    linklet_bson_store_u64le(p, (uint64_t)value);
    return true;
}

bool linklet_bson_append_timestamp(LinkletBson *bson, const char *key, uint32_t increment,
                                   uint32_t timestamp, LinkletBsonError *error) {
    uint8_t *p = linklet_bson_append_value_begin(bson, key, LINKLET_BSON_TYPE_TIMESTAMP, 8, error);
    if (!p) {
        return false;
    }
    linklet_bson_store_u32le(p, increment);
    linklet_bson_store_u32le(p + 4, timestamp);
    return true;
}

bool linklet_bson_append_decimal128(LinkletBson *bson, const char *key,
                                    const LinkletBsonDecimal128 *value, LinkletBsonError *error) {
    if (!value) {
        linklet_bson_set_error(error, "decimal128 value is NULL");
        return false;
    }
    uint8_t *p = linklet_bson_append_value_begin(bson, key, LINKLET_BSON_TYPE_DECIMAL128,
                                                 LINKLET_BSON_DECIMAL128_BYTE_LENGTH, error);
    if (!p) {
        return false;
    }
    memcpy(p, value->bytes, sizeof(value->bytes));
    return true;
}

bool linklet_bson_append_maxkey(LinkletBson *bson, const char *key, LinkletBsonError *error) {
    return linklet_bson_append_value_begin(bson, key, LINKLET_BSON_TYPE_MAXKEY, 0, error) != NULL;
}

bool linklet_bson_append_minkey(LinkletBson *bson, const char *key, LinkletBsonError *error) {
    return linklet_bson_append_value_begin(bson, key, LINKLET_BSON_TYPE_MINKEY, 0, error) != NULL;
}

bool linklet_bson_append_iterator(LinkletBson *bson, const LinkletBsonIterator *value,
                                  LinkletBsonError *error) {
    if (!value || !value->data || !value->key || !value->value ||
        value->next_offset < (size_t)(value->value - value->data)) {
        linklet_bson_set_error(error, "BSON iterator does not reference an element");
        return false;
    }
    const size_t value_size = value->next_offset - (size_t)(value->value - value->data);
    uint8_t *destination =
        linklet_bson_append_value_begin(bson, value->key, value->type, value_size, error);
    if (!destination) {
        return false;
    }
    if (value_size != 0) {
        memcpy(destination, value->value, value_size);
    }
    return true;
}

bool linklet_bson_merge(const LinkletBson *original, const LinkletBson *patch, LinkletBson *result,
                        LinkletBsonError *error) {
    if (!original || !patch || !result) {
        linklet_bson_set_error(error, "original, patch, and result documents are required");
        return false;
    }
    linklet_bson_init(result);
    LinkletBsonIterator iterator;
    if (linklet_bson_iterator_init(&iterator, original)) {
        do {
            LinkletBsonIterator replacement;
            if (!linklet_bson_find(patch, iterator.key, &replacement) &&
                !linklet_bson_append_iterator(result, &iterator, error)) {
                linklet_bson_destroy(result);
                return false;
            }
        } while (linklet_bson_iterator_next(&iterator));
    }
    if (linklet_bson_iterator_init(&iterator, patch)) {
        do {
            if (!linklet_bson_append_iterator(result, &iterator, error)) {
                linklet_bson_destroy(result);
                return false;
            }
        } while (linklet_bson_iterator_next(&iterator));
    }
    return true;
}

static bool linklet_bson_append_nested_begin(LinkletBson *bson, const char *key,
                                             LinkletBsonType type, LinkletBson *child,
                                             LinkletBsonError *error) {
    if (!child) {
        linklet_bson_set_error(error, "child document is NULL");
        return false;
    }

    if (!linklet_bson_append_value_begin(bson, key, type, 0, error)) {
        return false;
    }
    linklet_bson_init(child);
    return true;
}

static bool linklet_bson_append_nested_end(LinkletBson *bson, LinkletBson *child,
                                           LinkletBsonError *error) {
    if (!child) {
        linklet_bson_set_error(error, "child document is NULL");
        return false;
    }
    const uint8_t *data = linklet_bson_get_data(child);
    const size_t length = linklet_bson_get_length(child);

    if (bson->flags & LINKLET_BSON_FLAG_STATIC) {
        linklet_bson_set_error(error, "cannot append to a read-only (static) document");
        linklet_bson_destroy(child);
        return false;
    }

    if (length > (size_t)INT32_MAX - bson->length) {
        linklet_bson_set_error(error, "document exceeds the maximum BSON size");
        linklet_bson_destroy(child);
        return false;
    }
    const size_t new_len = bson->length + length;
    if (!linklet_bson_reserve_to(bson, new_len, error)) {
        linklet_bson_destroy(child);
        return false;
    }
    memcpy(bson->data + bson->length - 1, data, length);
    bson->data[new_len - 1] = 0x00;
    bson->length = new_len;
    linklet_bson_store_u32le(bson->data, (uint32_t)new_len);
    linklet_bson_destroy(child);
    return true;
}

bool linklet_bson_append_document_begin(LinkletBson *bson, const char *key, LinkletBson *child,
                                        LinkletBsonError *error) {
    return linklet_bson_append_nested_begin(bson, key, LINKLET_BSON_TYPE_DOCUMENT, child, error);
}

bool linklet_bson_append_document_end(LinkletBson *bson, LinkletBson *child,
                                      LinkletBsonError *error) {
    return linklet_bson_append_nested_end(bson, child, error);
}

bool linklet_bson_append_array_begin(LinkletBson *bson, const char *key, LinkletBson *child,
                                     LinkletBsonError *error) {
    return linklet_bson_append_nested_begin(bson, key, LINKLET_BSON_TYPE_ARRAY, child, error);
}

bool linklet_bson_append_array_end(LinkletBson *bson, LinkletBson *child, LinkletBsonError *error) {
    return linklet_bson_append_nested_end(bson, child, error);
}

static bool linklet_bson_array_key(const LinkletBson *array,
                                   char output[LINKLET_BSON_ARRAY_KEY_CAPACITY]) {
    const size_t index = linklet_bson_count_keys(array);
    return snprintf(output, LINKLET_BSON_ARRAY_KEY_CAPACITY, "%zu", index) > 0;
}

bool linklet_bson_array_append_double(LinkletBson *array, double value, LinkletBsonError *error) {
    char key[LINKLET_BSON_ARRAY_KEY_CAPACITY];
    if (!linklet_bson_array_key(array, key)) {
        return false;
    }
    return linklet_bson_append_double(array, key, value, error);
}

bool linklet_bson_array_append_utf8(LinkletBson *array, const char *value, int length,
                                    LinkletBsonError *error) {
    char key[LINKLET_BSON_ARRAY_KEY_CAPACITY];
    if (!linklet_bson_array_key(array, key)) {
        return false;
    }
    return linklet_bson_append_utf8(array, key, value, length, error);
}

bool linklet_bson_array_append_document(LinkletBson *array, const LinkletBson *value,
                                        LinkletBsonError *error) {
    char key[LINKLET_BSON_ARRAY_KEY_CAPACITY];
    if (!linklet_bson_array_key(array, key)) {
        return false;
    }
    return linklet_bson_append_document(array, key, value, error);
}

bool linklet_bson_array_append_bool(LinkletBson *array, bool value, LinkletBsonError *error) {
    char key[LINKLET_BSON_ARRAY_KEY_CAPACITY];
    if (!linklet_bson_array_key(array, key)) {
        return false;
    }
    return linklet_bson_append_bool(array, key, value, error);
}

bool linklet_bson_array_append_null(LinkletBson *array, LinkletBsonError *error) {
    char key[LINKLET_BSON_ARRAY_KEY_CAPACITY];
    if (!linklet_bson_array_key(array, key)) {
        return false;
    }
    return linklet_bson_append_null(array, key, error);
}

bool linklet_bson_array_append_oid(LinkletBson *array, const LinkletBsonOid *oid,
                                   LinkletBsonError *error) {
    char key[LINKLET_BSON_ARRAY_KEY_CAPACITY];
    if (!linklet_bson_array_key(array, key)) {
        return false;
    }
    return linklet_bson_append_oid(array, key, oid, error);
}

bool linklet_bson_array_append_int32(LinkletBson *array, int32_t value, LinkletBsonError *error) {
    char key[LINKLET_BSON_ARRAY_KEY_CAPACITY];
    if (!linklet_bson_array_key(array, key)) {
        return false;
    }
    return linklet_bson_append_int32(array, key, value, error);
}

bool linklet_bson_array_append_int64(LinkletBson *array, int64_t value, LinkletBsonError *error) {
    char key[LINKLET_BSON_ARRAY_KEY_CAPACITY];
    if (!linklet_bson_array_key(array, key)) {
        return false;
    }
    return linklet_bson_append_int64(array, key, value, error);
}

bool linklet_bson_array_append_date_time(LinkletBson *array, int64_t milliseconds,
                                         LinkletBsonError *error) {
    char key[LINKLET_BSON_ARRAY_KEY_CAPACITY];
    if (!linklet_bson_array_key(array, key)) {
        return false;
    }
    return linklet_bson_append_date_time(array, key, milliseconds, error);
}
