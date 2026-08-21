#include <string.h>

#include "bson_reader.h"
#include "bson_internal.h"

static const uint8_t *linklet_bson_read_cstring(const uint8_t *p, const uint8_t *end) {
    if (p == NULL || end == NULL || p > end) {
        return NULL;
    }
    return (const uint8_t *)memchr(p, 0, (size_t)(end - p));
}

static bool linklet_bson_iterator_decode_at(LinkletBsonIterator *iterator, size_t offset) {
    const uint8_t *end = iterator->data + iterator->length;
    if (offset + 1 > iterator->length) {
        return false;
    }
    const uint8_t type_byte = iterator->data[offset];
    if (type_byte == 0x00) {
        return false;
    }

    const uint8_t *key = iterator->data + offset + 1;
    const uint8_t *key_end = linklet_bson_read_cstring(key, end);
    if (!key_end) {
        return false;
    }
    const uint8_t *value = key_end + 1;
    if (value > end) {
        return false;
    }

    const LinkletBsonType type = (LinkletBsonType)type_byte;
    const size_t value_size = linklet_bson_value_size(type, value, end);

    if (value_size == 0 && type != LINKLET_BSON_TYPE_NULL && type != LINKLET_BSON_TYPE_UNDEFINED &&
        type != LINKLET_BSON_TYPE_MINKEY && type != LINKLET_BSON_TYPE_MAXKEY) {
        return false;
    }
    if (value + value_size > end) {
        return false;
    }

    iterator->offset = offset;
    iterator->type = type;
    iterator->key = (const char *)key;
    iterator->key_length = (size_t)(key_end - key);
    iterator->value = value;
    iterator->next_offset = (size_t)(value - iterator->data) + value_size;
    return true;
}

bool linklet_bson_iterator_init_from_data(LinkletBsonIterator *iterator, const uint8_t *data,
                                          size_t length) {
    if (!iterator) {
        return false;
    }
    iterator->data = data;
    iterator->length = length;
    iterator->offset = 0;
    iterator->next_offset = 0;
    iterator->type = LINKLET_BSON_TYPE_EOD;
    iterator->key = NULL;
    iterator->key_length = 0;
    iterator->value = NULL;
    if (!data || length < LINKLET_BSON_EMPTY_DOCUMENT_SIZE) {
        return false;
    }
    if (!linklet_bson_iterator_decode_at(iterator, 4)) {
        iterator->type = LINKLET_BSON_TYPE_EOD;
        iterator->key = NULL;
        iterator->key_length = 0;
        iterator->value = NULL;
        return false;
    }
    return true;
}

bool linklet_bson_iterator_init(LinkletBsonIterator *iterator, const LinkletBson *bson) {
    if (!iterator || !bson) {
        return false;
    }
    return linklet_bson_iterator_init_from_data(iterator, linklet_bson_get_data(bson),
                                                linklet_bson_get_length(bson));
}

bool linklet_bson_iterator_next(LinkletBsonIterator *iterator) {
    if (!iterator || iterator->type == LINKLET_BSON_TYPE_EOD) {
        return false;
    }
    if (!linklet_bson_iterator_decode_at(iterator, iterator->next_offset)) {
        iterator->type = LINKLET_BSON_TYPE_EOD;
        iterator->key = NULL;
        iterator->key_length = 0;
        iterator->value = NULL;
        return false;
    }
    return true;
}

const char *linklet_bson_iterator_key(const LinkletBsonIterator *iterator) {
    return iterator ? iterator->key : NULL;
}

size_t linklet_bson_iterator_key_length(const LinkletBsonIterator *iterator) {
    return iterator ? iterator->key_length : 0;
}

LinkletBsonType linklet_bson_iterator_type(const LinkletBsonIterator *iterator) {
    return iterator ? iterator->type : LINKLET_BSON_TYPE_EOD;
}

static bool linklet_bson_iterator_is(const LinkletBsonIterator *iterator, LinkletBsonType type) {
    return iterator && iterator->type == type && iterator->value;
}

bool linklet_bson_iterator_double(const LinkletBsonIterator *iterator, double *output) {
    if (!linklet_bson_iterator_is(iterator, LINKLET_BSON_TYPE_DOUBLE)) {
        return false;
    }
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length, 8)) {
        return false;
    }
    if (output) {
        *output = linklet_bson_load_double_le(iterator->value);
    }
    return true;
}

bool linklet_bson_iterator_utf8(const LinkletBsonIterator *iterator, const char **string,
                                size_t *length) {
    if (!linklet_bson_iterator_is(iterator, LINKLET_BSON_TYPE_UTF8)) {
        return false;
    }
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length, 4)) {
        return false;
    }
    const uint32_t n = linklet_bson_load_u32le(iterator->value);
    if (n < 1) {
        return false;
    }
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length,
                                      4 + (size_t)n)) {
        return false;
    }
    if (string) {
        *string = (const char *)(iterator->value + 4);
    }
    if (length) {
        *length = (size_t)n - 1;
    }
    return true;
}

bool linklet_bson_iterator_document(const LinkletBsonIterator *iterator, const uint8_t **data,
                                    size_t *length) {
    if (!linklet_bson_iterator_is(iterator, LINKLET_BSON_TYPE_DOCUMENT)) {
        return false;
    }
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length, 4)) {
        return false;
    }
    if (data) {
        *data = iterator->value;
    }
    if (length) {
        *length = (size_t)linklet_bson_load_u32le(iterator->value);
    }
    return true;
}

bool linklet_bson_iterator_array(const LinkletBsonIterator *iterator, const uint8_t **data,
                                 size_t *length) {
    if (!linklet_bson_iterator_is(iterator, LINKLET_BSON_TYPE_ARRAY)) {
        return false;
    }
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length, 4)) {
        return false;
    }
    if (data) {
        *data = iterator->value;
    }
    if (length) {
        *length = (size_t)linklet_bson_load_u32le(iterator->value);
    }
    return true;
}

bool linklet_bson_iterator_binary(const LinkletBsonIterator *iterator, LinkletBsonSubtype *subtype,
                                  const uint8_t **data, size_t *length) {
    if (!linklet_bson_iterator_is(iterator, LINKLET_BSON_TYPE_BINARY)) {
        return false;
    }
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length, 5)) {
        return false;
    }
    const uint32_t n = linklet_bson_load_u32le(iterator->value);
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length,
                                      5 + (size_t)n)) {
        return false;
    }
    if (subtype) {
        *subtype = (LinkletBsonSubtype)iterator->value[4];
    }
    if (data) {
        *data = iterator->value + 5;
    }
    if (length) {
        *length = (size_t)n;
    }
    return true;
}

bool linklet_bson_iterator_oid(const LinkletBsonIterator *iterator, LinkletBsonOid *output) {
    if (!linklet_bson_iterator_is(iterator, LINKLET_BSON_TYPE_OID)) {
        return false;
    }
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length,
                                      LINKLET_BSON_OID_BYTE_LENGTH)) {
        return false;
    }
    if (output) {
        memcpy(output->bytes, iterator->value, sizeof(output->bytes));
    }
    return true;
}

bool linklet_bson_iterator_bool(const LinkletBsonIterator *iterator, bool *output) {
    if (!linklet_bson_iterator_is(iterator, LINKLET_BSON_TYPE_BOOL)) {
        return false;
    }
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length, 1)) {
        return false;
    }
    if (output) {
        *output = iterator->value[0] != 0x00;
    }
    return true;
}

bool linklet_bson_iterator_date_time(const LinkletBsonIterator *iterator, int64_t *milliseconds) {
    if (!linklet_bson_iterator_is(iterator, LINKLET_BSON_TYPE_DATE_TIME)) {
        return false;
    }
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length, 8)) {
        return false;
    }
    if (milliseconds) {
        *milliseconds = linklet_bson_load_i64le(iterator->value);
    }
    return true;
}

bool linklet_bson_iterator_regex(const LinkletBsonIterator *iterator, const char **pattern,
                                 const char **options) {
    if (!linklet_bson_iterator_is(iterator, LINKLET_BSON_TYPE_REGEX)) {
        return false;
    }
    const uint8_t *end = iterator->data + iterator->length;
    const uint8_t *opts = linklet_bson_read_cstring(iterator->value, end);
    if (!opts) {
        return false;
    }
    const uint8_t *opts_end = linklet_bson_read_cstring(opts + 1, end);
    if (!opts_end) {
        return false;
    }
    if (pattern) {
        *pattern = (const char *)iterator->value;
    }
    if (options) {
        *options = (const char *)(opts + 1);
    }
    return true;
}

bool linklet_bson_iterator_dbpointer(const LinkletBsonIterator *iterator, const char **collection,
                                     LinkletBsonOid *oid) {
    if (!linklet_bson_iterator_is(iterator, LINKLET_BSON_TYPE_DBPOINTER)) {
        return false;
    }
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length, 4)) {
        return false;
    }
    const uint32_t n = linklet_bson_load_u32le(iterator->value);
    if (n < 1) {
        return false;
    }
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length,
                                      4 + (size_t)n + LINKLET_BSON_OID_BYTE_LENGTH)) {
        return false;
    }
    if (collection) {
        *collection = (const char *)(iterator->value + 4);
    }
    if (oid) {
        memcpy(oid->bytes, iterator->value + 4 + n, sizeof(oid->bytes));
    }
    return true;
}

bool linklet_bson_iterator_code(const LinkletBsonIterator *iterator, const char **code,
                                size_t *length) {
    if (!linklet_bson_iterator_is(iterator, LINKLET_BSON_TYPE_CODE)) {
        return false;
    }
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length, 4)) {
        return false;
    }
    const uint32_t n = linklet_bson_load_u32le(iterator->value);
    if (n < 1) {
        return false;
    }
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length,
                                      4 + (size_t)n)) {
        return false;
    }
    if (code) {
        *code = (const char *)(iterator->value + 4);
    }
    if (length) {
        *length = (size_t)n - 1;
    }
    return true;
}

bool linklet_bson_iterator_symbol(const LinkletBsonIterator *iterator, const char **symbol,
                                  size_t *length) {
    if (!linklet_bson_iterator_is(iterator, LINKLET_BSON_TYPE_SYMBOL)) {
        return false;
    }
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length, 4)) {
        return false;
    }
    const uint32_t n = linklet_bson_load_u32le(iterator->value);
    if (n < 1) {
        return false;
    }
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length,
                                      4 + (size_t)n)) {
        return false;
    }
    if (symbol) {
        *symbol = (const char *)(iterator->value + 4);
    }
    if (length) {
        *length = (size_t)n - 1;
    }
    return true;
}

bool linklet_bson_iterator_code_with_scope(const LinkletBsonIterator *iterator, const char **code,
                                           size_t *code_length, const uint8_t **scope,
                                           size_t *scope_length) {
    if (!linklet_bson_iterator_is(iterator, LINKLET_BSON_TYPE_CODEWSCOPE)) {
        return false;
    }
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length, 8)) {
        return false;
    }
    const uint32_t code_str_len = linklet_bson_load_u32le(iterator->value + 4);
    if (code_str_len < 1) {
        return false;
    }
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length,
                                      8 + (size_t)code_str_len + 4)) {
        return false;
    }
    if (code) {
        *code = (const char *)(iterator->value + 8);
    }
    if (code_length) {
        *code_length = (size_t)code_str_len - 1;
    }
    const uint8_t *scope_doc = iterator->value + 8 + code_str_len;
    const uint32_t scope_doc_len = linklet_bson_load_u32le(scope_doc);
    if (scope) {
        *scope = scope_doc;
    }
    if (scope_length) {
        *scope_length = (size_t)scope_doc_len;
    }
    return true;
}

bool linklet_bson_iterator_int32(const LinkletBsonIterator *iterator, int32_t *output) {
    if (!linklet_bson_iterator_is(iterator, LINKLET_BSON_TYPE_INT32)) {
        return false;
    }
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length, 4)) {
        return false;
    }
    if (output) {
        *output = linklet_bson_load_i32le(iterator->value);
    }
    return true;
}

bool linklet_bson_iterator_int64(const LinkletBsonIterator *iterator, int64_t *output) {
    if (!linklet_bson_iterator_is(iterator, LINKLET_BSON_TYPE_INT64)) {
        return false;
    }
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length, 8)) {
        return false;
    }
    if (output) {
        *output = linklet_bson_load_i64le(iterator->value);
    }
    return true;
}

bool linklet_bson_iterator_timestamp(const LinkletBsonIterator *iterator, uint32_t *increment,
                                     uint32_t *timestamp) {
    if (!linklet_bson_iterator_is(iterator, LINKLET_BSON_TYPE_TIMESTAMP)) {
        return false;
    }
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length, 8)) {
        return false;
    }
    if (increment) {
        *increment = linklet_bson_load_u32le(iterator->value);
    }
    if (timestamp) {
        *timestamp = linklet_bson_load_u32le(iterator->value + 4);
    }
    return true;
}

bool linklet_bson_iterator_decimal128(const LinkletBsonIterator *iterator,
                                      LinkletBsonDecimal128 *output) {
    if (!linklet_bson_iterator_is(iterator, LINKLET_BSON_TYPE_DECIMAL128)) {
        return false;
    }
    if (!linklet_bson_bytes_available(iterator->value, iterator->data + iterator->length,
                                      LINKLET_BSON_DECIMAL128_BYTE_LENGTH)) {
        return false;
    }
    if (output) {
        memcpy(output->bytes, iterator->value, sizeof(output->bytes));
    }
    return true;
}

bool linklet_bson_find(const LinkletBson *bson, const char *key, LinkletBsonIterator *iterator) {
    if (!bson || !key || !iterator) {
        return false;
    }
    if (!linklet_bson_iterator_init(iterator, bson)) {
        return false;
    }
    while (iterator->type != LINKLET_BSON_TYPE_EOD) {
        if (iterator->key && strcmp(iterator->key, key) == 0) {
            return true;
        }
        if (!linklet_bson_iterator_next(iterator)) {
            break;
        }
    }
    return false;
}

bool linklet_bson_has_field(const LinkletBson *bson, const char *key) {
    LinkletBsonIterator iterator;
    return linklet_bson_find(bson, key, &iterator);
}

size_t linklet_bson_count_keys(const LinkletBson *bson) {
    LinkletBsonIterator iterator;
    if (!linklet_bson_iterator_init(&iterator, bson)) {
        return 0;
    }
    size_t count = 0;
    do {
        count++;
    } while (linklet_bson_iterator_next(&iterator));
    return count;
}

bool linklet_bson_get_double(const LinkletBson *bson, const char *key, double *output) {
    LinkletBsonIterator iterator;
    return linklet_bson_find(bson, key, &iterator) &&
           linklet_bson_iterator_double(&iterator, output);
}

bool linklet_bson_get_utf8(const LinkletBson *bson, const char *key, const char **string,
                           size_t *length) {
    LinkletBsonIterator iterator;
    return linklet_bson_find(bson, key, &iterator) &&
           linklet_bson_iterator_utf8(&iterator, string, length);
}

bool linklet_bson_get_document(const LinkletBson *bson, const char *key, const uint8_t **data,
                               size_t *length) {
    LinkletBsonIterator iterator;
    return linklet_bson_find(bson, key, &iterator) &&
           linklet_bson_iterator_document(&iterator, data, length);
}

bool linklet_bson_get_array(const LinkletBson *bson, const char *key, const uint8_t **data,
                            size_t *length) {
    LinkletBsonIterator iterator;
    return linklet_bson_find(bson, key, &iterator) &&
           linklet_bson_iterator_array(&iterator, data, length);
}

bool linklet_bson_get_bool(const LinkletBson *bson, const char *key, bool *output) {
    LinkletBsonIterator iterator;
    return linklet_bson_find(bson, key, &iterator) && linklet_bson_iterator_bool(&iterator, output);
}

bool linklet_bson_get_date_time(const LinkletBson *bson, const char *key, int64_t *milliseconds) {
    LinkletBsonIterator iterator;
    return linklet_bson_find(bson, key, &iterator) &&
           linklet_bson_iterator_date_time(&iterator, milliseconds);
}

bool linklet_bson_get_oid(const LinkletBson *bson, const char *key, LinkletBsonOid *output) {
    LinkletBsonIterator iterator;
    return linklet_bson_find(bson, key, &iterator) && linklet_bson_iterator_oid(&iterator, output);
}

bool linklet_bson_get_int32(const LinkletBson *bson, const char *key, int32_t *output) {
    LinkletBsonIterator iterator;
    return linklet_bson_find(bson, key, &iterator) &&
           linklet_bson_iterator_int32(&iterator, output);
}

bool linklet_bson_get_int64(const LinkletBson *bson, const char *key, int64_t *output) {
    LinkletBsonIterator iterator;
    return linklet_bson_find(bson, key, &iterator) &&
           linklet_bson_iterator_int64(&iterator, output);
}

bool linklet_bson_get_timestamp(const LinkletBson *bson, const char *key, uint32_t *increment,
                                uint32_t *timestamp) {
    LinkletBsonIterator iterator;
    return linklet_bson_find(bson, key, &iterator) &&
           linklet_bson_iterator_timestamp(&iterator, increment, timestamp);
}
