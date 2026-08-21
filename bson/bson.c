#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bson.h"
#include "bson_internal.h"

void linklet_bson_set_error(LinkletBsonError *error, const char *format, ...) {
    if (!error) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
}

bool linklet_bson_utf8_validate(const uint8_t *data, size_t length) {
    if (length != 0 && !data) {
        return false;
    }
    size_t i = 0;
    while (i < length) {
        const uint8_t c = data[i];
        if (c <= 0x7F) {
            i += 1;
        } else if (c >= 0xC2 && c <= 0xDF) {
            if (i + 1 >= length || (data[i + 1] & 0xC0) != 0x80) {
                return false;
            }
            i += 2;
        } else if (c == 0xE0) {
            if (i + 2 >= length || (data[i + 1] & 0xE0) != 0xA0 || (data[i + 2] & 0xC0) != 0x80) {
                return false;
            }
            i += 3;
        } else if ((c >= 0xE1 && c <= 0xEC) || (c >= 0xEE && c <= 0xEF)) {
            if (i + 2 >= length || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80) {
                return false;
            }
            i += 3;
        } else if (c == 0xED) {
            if (i + 2 >= length || (data[i + 1] & 0xE0) != 0x80 || (data[i + 2] & 0xC0) != 0x80) {
                return false;
            }
            i += 3;
        } else if (c == 0xF0) {
            if (i + 3 >= length || (data[i + 1] & 0xF0) != 0x90 || (data[i + 2] & 0xC0) != 0x80 ||
                (data[i + 3] & 0xC0) != 0x80) {
                return false;
            }
            i += 4;
        } else if (c >= 0xF1 && c <= 0xF3) {
            if (i + 3 >= length || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80 ||
                (data[i + 3] & 0xC0) != 0x80) {
                return false;
            }
            i += 4;
        } else if (c == 0xF4) {
            if (i + 3 >= length || (data[i + 1] & 0xF0) != 0x80 || (data[i + 2] & 0xC0) != 0x80 ||
                (data[i + 3] & 0xC0) != 0x80) {
                return false;
            }
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

size_t linklet_bson_value_size(LinkletBsonType type, const uint8_t *value, const uint8_t *end) {
    if (!linklet_bson_bytes_available(value, end, 0)) {
        return 0;
    }

    switch (type) {
    case LINKLET_BSON_TYPE_DOUBLE:
        return linklet_bson_bytes_available(value, end, 8) ? 8 : 0;

    case LINKLET_BSON_TYPE_UTF8:
    case LINKLET_BSON_TYPE_CODE:
    case LINKLET_BSON_TYPE_SYMBOL: {
        if (!linklet_bson_bytes_available(value, end, 4)) {
            return 0;
        }
        const uint32_t n = linklet_bson_load_u32le(value);
        if (n < 1 || n > INT32_MAX) {
            return 0;
        }
        return linklet_bson_bytes_available(value, end, 4 + (size_t)n) ? 4 + (size_t)n : 0;
    }

    case LINKLET_BSON_TYPE_DOCUMENT:
    case LINKLET_BSON_TYPE_ARRAY: {
        if (!linklet_bson_bytes_available(value, end, 4)) {
            return 0;
        }
        const uint32_t n = linklet_bson_load_u32le(value);
        if (n < 5 || n > INT32_MAX) {
            return 0;
        }
        return linklet_bson_bytes_available(value, end, (size_t)n) ? (size_t)n : 0;
    }

    case LINKLET_BSON_TYPE_BINARY: {
        if (!linklet_bson_bytes_available(value, end, 5)) {
            return 0;
        }
        const uint32_t n = linklet_bson_load_u32le(value);
        if (n > INT32_MAX) {
            return 0;
        }
        return linklet_bson_bytes_available(value, end, 5 + (size_t)n) ? 5 + (size_t)n : 0;
    }

    case LINKLET_BSON_TYPE_UNDEFINED:
    case LINKLET_BSON_TYPE_NULL:
    case LINKLET_BSON_TYPE_MINKEY:
    case LINKLET_BSON_TYPE_MAXKEY:
        return 0;

    case LINKLET_BSON_TYPE_OID:
        return linklet_bson_bytes_available(value, end, 12) ? 12 : 0;

    case LINKLET_BSON_TYPE_BOOL:
        return linklet_bson_bytes_available(value, end, 1) ? 1 : 0;

    case LINKLET_BSON_TYPE_DATE_TIME:
        return linklet_bson_bytes_available(value, end, 8) ? 8 : 0;

    case LINKLET_BSON_TYPE_REGEX: {
        const uint8_t *nul = (const uint8_t *)memchr(value, 0, (size_t)(end - value));
        if (!nul) {
            return 0;
        }
        const uint8_t *opts = nul + 1;
        if (opts > end) {
            return 0;
        }
        const uint8_t *nul2 = (const uint8_t *)memchr(opts, 0, (size_t)(end - opts));
        if (!nul2) {
            return 0;
        }
        return (size_t)(nul2 - value) + 1;
    }

    case LINKLET_BSON_TYPE_DBPOINTER: {
        if (!linklet_bson_bytes_available(value, end, 4)) {
            return 0;
        }
        const uint32_t n = linklet_bson_load_u32le(value);
        if (n < 1 || n > INT32_MAX) {
            return 0;
        }
        const size_t total = 4 + (size_t)n + 12;
        return linklet_bson_bytes_available(value, end, total) ? total : 0;
    }

    case LINKLET_BSON_TYPE_CODEWSCOPE: {
        if (!linklet_bson_bytes_available(value, end, 4)) {
            return 0;
        }
        const uint32_t n = linklet_bson_load_u32le(value);

        if (n < 14 || n > INT32_MAX) {
            return 0;
        }
        return linklet_bson_bytes_available(value, end, (size_t)n) ? (size_t)n : 0;
    }

    case LINKLET_BSON_TYPE_INT32:
        return linklet_bson_bytes_available(value, end, 4) ? 4 : 0;

    case LINKLET_BSON_TYPE_TIMESTAMP:
    case LINKLET_BSON_TYPE_INT64:
        return linklet_bson_bytes_available(value, end, 8) ? 8 : 0;

    case LINKLET_BSON_TYPE_DECIMAL128:
        return linklet_bson_bytes_available(value, end, 16) ? 16 : 0;

    default:
        return 0;
    }
}

const uint8_t *linklet_bson_empty_doc_bytes(size_t *length) {
    static const uint8_t empty[LINKLET_BSON_EMPTY_DOCUMENT_SIZE] = {0x05, 0x00, 0x00, 0x00, 0x00};
    if (length) {
        *length = sizeof(empty);
    }
    return empty;
}

void linklet_bson_init(LinkletBson *bson) {
    if (!bson) {
        return;
    }
    *bson = (LinkletBson){0};
}

void linklet_bson_init_static(LinkletBson *bson, const uint8_t *data, size_t length) {
    if (!bson) {
        return;
    }
    bson->data = (uint8_t *)(uintptr_t)data;
    bson->length = data ? length : 0;
    bson->capacity = 0;
    bson->flags = LINKLET_BSON_FLAG_STATIC;
}

void linklet_bson_clear(LinkletBson *bson) {
    if (!bson) {
        return;
    }
    if (!(bson->flags & LINKLET_BSON_FLAG_STATIC) && bson->data) {
        free(bson->data);
    }
    *bson = (LinkletBson){0};
}

void linklet_bson_destroy(LinkletBson *bson) {
    linklet_bson_clear(bson);
}

void linklet_bson_free(LinkletBson *bson) {
    if (!bson) {
        return;
    }
    linklet_bson_destroy(bson);
    free(bson);
}

LinkletBson *linklet_bson_new(void) {
    return (LinkletBson *)calloc(1, sizeof(LinkletBson));
}

LinkletBson *linklet_bson_new_from_data(const uint8_t *data, size_t length) {
    LinkletBson *bson = linklet_bson_new();
    if (!bson) {
        return NULL;
    }
    if (length == 0) {
        return bson;
    }
    if (!data) {
        free(bson);
        return NULL;
    }
    bson->data = (uint8_t *)malloc(length);
    if (!bson->data) {
        free(bson);
        return NULL;
    }
    memcpy(bson->data, data, length);
    bson->length = length;
    bson->capacity = length;
    bson->flags = LINKLET_BSON_FLAG_NONE;
    return bson;
}

LinkletBson *linklet_bson_copy(const LinkletBson *bson) {
    if (!bson) {
        return NULL;
    }
    return linklet_bson_new_from_data(linklet_bson_get_data(bson), linklet_bson_get_length(bson));
}

LinkletBson linklet_bson_view(const uint8_t *data, size_t length) {
    LinkletBson bson;
    linklet_bson_init_static(&bson, data, length);
    return bson;
}

const uint8_t *linklet_bson_get_data(const LinkletBson *bson) {
    if (!bson || bson->length == 0) {
        return linklet_bson_empty_doc_bytes(NULL);
    }
    return bson->data;
}

size_t linklet_bson_get_length(const LinkletBson *bson) {
    if (!bson || bson->length == 0) {
        return LINKLET_BSON_EMPTY_DOCUMENT_SIZE;
    }
    return bson->length;
}

bool linklet_bson_empty(const LinkletBson *bson) {
    return !bson || bson->length == 0;
}

static bool linklet_bson_type_is_valid(LinkletBsonType type) {
    switch (type) {
    case LINKLET_BSON_TYPE_DOUBLE:
    case LINKLET_BSON_TYPE_UTF8:
    case LINKLET_BSON_TYPE_DOCUMENT:
    case LINKLET_BSON_TYPE_ARRAY:
    case LINKLET_BSON_TYPE_BINARY:
    case LINKLET_BSON_TYPE_UNDEFINED:
    case LINKLET_BSON_TYPE_OID:
    case LINKLET_BSON_TYPE_BOOL:
    case LINKLET_BSON_TYPE_DATE_TIME:
    case LINKLET_BSON_TYPE_NULL:
    case LINKLET_BSON_TYPE_REGEX:
    case LINKLET_BSON_TYPE_DBPOINTER:
    case LINKLET_BSON_TYPE_CODE:
    case LINKLET_BSON_TYPE_SYMBOL:
    case LINKLET_BSON_TYPE_CODEWSCOPE:
    case LINKLET_BSON_TYPE_INT32:
    case LINKLET_BSON_TYPE_TIMESTAMP:
    case LINKLET_BSON_TYPE_INT64:
    case LINKLET_BSON_TYPE_DECIMAL128:
    case LINKLET_BSON_TYPE_MAXKEY:
    case LINKLET_BSON_TYPE_MINKEY:
        return true;
    default:
        return false;
    }
}

static bool linklet_bson_type_has_no_value(LinkletBsonType type) {
    return type == LINKLET_BSON_TYPE_UNDEFINED || type == LINKLET_BSON_TYPE_NULL ||
           type == LINKLET_BSON_TYPE_MINKEY || type == LINKLET_BSON_TYPE_MAXKEY;
}

static bool linklet_bson_validate_string(const uint8_t *value, const uint8_t *end,
                                         LinkletBsonError *error) {
    if (!linklet_bson_bytes_available(value, end, 4)) {
        linklet_bson_set_error(error, "truncated string length");
        return false;
    }
    const uint32_t n = linklet_bson_load_u32le(value);
    if (n < 1 || n > INT32_MAX) {
        linklet_bson_set_error(error, "invalid string length");
        return false;
    }
    if (!linklet_bson_bytes_available(value, end, 4 + (size_t)n)) {
        linklet_bson_set_error(error, "string exceeds document bounds");
        return false;
    }
    const uint8_t *string = value + 4;
    const size_t str_len = (size_t)n - 1;
    if (string[str_len] != 0x00) {
        linklet_bson_set_error(error, "string is not NUL-terminated");
        return false;
    }
    if (!linklet_bson_utf8_validate(string, str_len)) {
        linklet_bson_set_error(error, "string is not valid UTF-8");
        return false;
    }
    return true;
}

static bool linklet_bson_validate_document_range(const uint8_t *data, size_t length, int depth,
                                                 bool is_array, LinkletBsonError *error);

static bool linklet_bson_validate_code_with_scope(const uint8_t *value, const uint8_t *end,
                                                  int depth, LinkletBsonError *error) {
    if (!linklet_bson_bytes_available(value, end, 4)) {
        linklet_bson_set_error(error, "truncated code-with-scope");
        return false;
    }
    const uint32_t total = linklet_bson_load_u32le(value);
    if (total < 14 || total > INT32_MAX) {
        linklet_bson_set_error(error, "invalid code-with-scope length");
        return false;
    }
    if (!linklet_bson_bytes_available(value, end, (size_t)total)) {
        linklet_bson_set_error(error, "code-with-scope exceeds document bounds");
        return false;
    }
    const uint8_t *scope_end = value + total;

    if (!linklet_bson_bytes_available(value, scope_end, 8)) {
        linklet_bson_set_error(error, "truncated code-with-scope code string");
        return false;
    }
    const uint32_t code_length = linklet_bson_load_u32le(value + 4);
    if (code_length < 1 || code_length > INT32_MAX) {
        linklet_bson_set_error(error, "invalid code-with-scope code length");
        return false;
    }
    if (!linklet_bson_bytes_available(value + 4, scope_end, 4 + (size_t)code_length)) {
        linklet_bson_set_error(error, "code-with-scope code exceeds its bounds");
        return false;
    }
    const uint8_t *code = value + 8;
    if (code[code_length - 1] != 0x00 ||
        !linklet_bson_utf8_validate(code, (size_t)code_length - 1)) {
        linklet_bson_set_error(error, "code-with-scope code is not valid UTF-8");
        return false;
    }

    const uint8_t *scope = value + 8 + code_length;
    if (scope >= scope_end) {
        linklet_bson_set_error(error, "code-with-scope is missing its scope document");
        return false;
    }
    if (!linklet_bson_bytes_available(scope, scope_end, 4)) {
        linklet_bson_set_error(error, "truncated code-with-scope scope");
        return false;
    }
    const uint32_t scope_length = linklet_bson_load_u32le(scope);
    if (!linklet_bson_bytes_available(scope, scope_end, (size_t)scope_length) ||
        scope + (size_t)scope_length != scope_end) {
        linklet_bson_set_error(error, "code-with-scope scope length mismatch");
        return false;
    }
    return linklet_bson_validate_document_range(scope, (size_t)scope_length, depth + 1, false,
                                                error);
}

static bool linklet_bson_validate_value(LinkletBsonType type, const uint8_t *value,
                                        const uint8_t *end, int depth, LinkletBsonError *error) {
    switch (type) {
    case LINKLET_BSON_TYPE_DOCUMENT:
    case LINKLET_BSON_TYPE_ARRAY: {
        const uint32_t n = linklet_bson_load_u32le(value);
        return linklet_bson_validate_document_range(value, (size_t)n, depth + 1,
                                                    type == LINKLET_BSON_TYPE_ARRAY, error);
    }
    case LINKLET_BSON_TYPE_UTF8:
    case LINKLET_BSON_TYPE_CODE:
    case LINKLET_BSON_TYPE_SYMBOL:
        return linklet_bson_validate_string(value, end, error);
    case LINKLET_BSON_TYPE_DBPOINTER: {

        if (!linklet_bson_validate_string(value, end, error)) {
            return false;
        }
        const uint32_t n = linklet_bson_load_u32le(value);
        if (!linklet_bson_bytes_available(value, end, 4 + (size_t)n + 12)) {
            linklet_bson_set_error(error, "dbpointer exceeds document bounds");
            return false;
        }
        return true;
    }
    case LINKLET_BSON_TYPE_CODEWSCOPE:
        return linklet_bson_validate_code_with_scope(value, end, depth, error);
    default:
        return true;
    }
}

static bool linklet_bson_validate_document_range(const uint8_t *data, size_t length, int depth,
                                                 bool is_array, LinkletBsonError *error) {
    if (length < LINKLET_BSON_EMPTY_DOCUMENT_SIZE) {
        linklet_bson_set_error(error, "invalid document length");
        return false;
    }
    if (depth > LINKLET_BSON_DEFAULT_MAX_DEPTH) {
        linklet_bson_set_error(error, "document nesting exceeds %d levels",
                               LINKLET_BSON_DEFAULT_MAX_DEPTH);
        return false;
    }
    if (data[length - 1] != 0x00) {
        linklet_bson_set_error(error, "document is not NUL-terminated");
        return false;
    }

    const uint8_t *end = data + length;
    size_t offset = 4;
    uint32_t array_index = 0;

    while (offset < length) {
        const uint8_t type_byte = data[offset];
        if (type_byte == 0x00) {
            if (offset != length - 1) {
                linklet_bson_set_error(error, "extra bytes after document terminator");
                return false;
            }
            return true;
        }

        const LinkletBsonType type = (LinkletBsonType)type_byte;
        if (!linklet_bson_type_is_valid(type)) {
            linklet_bson_set_error(error, "unknown element type 0x%02x", type_byte);
            return false;
        }

        const uint8_t *key = data + offset + 1;
        if (key >= end) {
            linklet_bson_set_error(error, "truncated element key");
            return false;
        }
        const uint8_t *key_end = (const uint8_t *)memchr(key, 0, (size_t)(end - key));
        if (!key_end) {
            linklet_bson_set_error(error, "element key is not NUL-terminated");
            return false;
        }
        const size_t key_length = (size_t)(key_end - key);
        if (key_length == 0) {
            linklet_bson_set_error(error, "element key must not be empty");
            return false;
        }
        if (!linklet_bson_utf8_validate(key, key_length)) {
            linklet_bson_set_error(error, "element key is not valid UTF-8");
            return false;
        }

        if (is_array) {
            char expected[24];
            const int expected_len = snprintf(expected, sizeof(expected), "%u", array_index);
            if (expected_len < 0 || (size_t)expected_len != key_length ||
                memcmp(expected, key, key_length) != 0) {
                linklet_bson_set_error(error,
                                       "array keys must be sequential integers starting at 0");
                return false;
            }
            array_index++;
        }

        const uint8_t *value = key_end + 1;
        if (value > end) {
            linklet_bson_set_error(error, "truncated element value");
            return false;
        }

        const size_t value_size = linklet_bson_value_size(type, value, end);
        if (value_size == 0 && !linklet_bson_type_has_no_value(type)) {
            linklet_bson_set_error(error, "element '%s' has a truncated value", (const char *)key);
            return false;
        }
        const uint8_t *next = value + value_size;
        if (next > end) {
            linklet_bson_set_error(error, "element value exceeds document bounds");
            return false;
        }

        if (!linklet_bson_validate_value(type, value, end, depth, error)) {
            return false;
        }

        offset = (size_t)(next - data);
    }

    linklet_bson_set_error(error, "document is missing its terminator");
    return false;
}

bool linklet_bson_validate(const uint8_t *data, size_t length, LinkletBsonError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!data || length < LINKLET_BSON_EMPTY_DOCUMENT_SIZE) {
        linklet_bson_set_error(error,
                               "document is NULL or shorter than the minimum BSON document size");
        return false;
    }
    const uint32_t declared = linklet_bson_load_u32le(data);
    if (declared != length) {
        linklet_bson_set_error(error,
                               "declared document length %u does not match buffer length %zu",
                               declared, length);
        return false;
    }
    return linklet_bson_validate_document_range(data, length, 0, false, error);
}

bool linklet_bson_validate_document(const LinkletBson *bson, LinkletBsonError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!bson) {
        linklet_bson_set_error(error, "document is NULL");
        return false;
    }
    return linklet_bson_validate(linklet_bson_get_data(bson), linklet_bson_get_length(bson), error);
}

static uint64_t linklet_bson_prng_state = 0;
static uint32_t linklet_bson_oid_counter_value = 0;

static uint64_t linklet_bson_prng_next(void) {
    if (linklet_bson_prng_state == 0) {
        linklet_bson_prng_state = (uint64_t)time(NULL) ^
                                  ((uint64_t)(uintptr_t)&linklet_bson_prng_state << 32) ^
                                  UINT64_C(0x9E3779B97F4A7C15);
    }
    uint64_t z = (linklet_bson_prng_state += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

void linklet_bson_oid_init_from_time(LinkletBsonOid *oid, uint32_t time_sec) {
    if (!oid) {
        return;
    }
    const uint64_t r = linklet_bson_prng_next();
    oid->bytes[0] = (uint8_t)(time_sec >> 24);
    oid->bytes[1] = (uint8_t)(time_sec >> 16);
    oid->bytes[2] = (uint8_t)(time_sec >> 8);
    oid->bytes[3] = (uint8_t)(time_sec);
    oid->bytes[4] = (uint8_t)(r);
    oid->bytes[5] = (uint8_t)(r >> 8);
    oid->bytes[6] = (uint8_t)(r >> 16);
    oid->bytes[7] = (uint8_t)(r >> 24);
    oid->bytes[8] = (uint8_t)(r >> 32);
    const uint32_t counter = linklet_bson_oid_counter_value++;
    oid->bytes[9] = (uint8_t)(counter >> 16);
    oid->bytes[10] = (uint8_t)(counter >> 8);
    oid->bytes[11] = (uint8_t)(counter);
}

void linklet_bson_oid_init(LinkletBsonOid *oid) {
    linklet_bson_oid_init_from_time(oid, (uint32_t)time(NULL));
}

uint32_t linklet_bson_oid_time(const LinkletBsonOid *oid) {
    if (!oid) {
        return 0;
    }
    return ((uint32_t)oid->bytes[0] << 24) | ((uint32_t)oid->bytes[1] << 16) |
           ((uint32_t)oid->bytes[2] << 8) | (uint32_t)oid->bytes[3];
}

static int linklet_bson_hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

void linklet_bson_oid_to_string(const LinkletBsonOid *oid,
                                char output[LINKLET_BSON_OID_STRING_CAPACITY]) {
    static const char hex[] = "0123456789abcdef";
    if (!oid || !output) {
        return;
    }
    for (int i = 0; i < LINKLET_BSON_OID_BYTE_LENGTH; i++) {
        output[i * 2] = hex[oid->bytes[i] >> 4];
        output[i * 2 + 1] = hex[oid->bytes[i] & 0x0F];
    }
    output[LINKLET_BSON_OID_STRING_LENGTH] = '\0';
}

bool linklet_bson_oid_from_string(const char *hex, LinkletBsonOid *oid) {
    if (!hex || !oid) {
        return false;
    }
    if (strlen(hex) != LINKLET_BSON_OID_STRING_LENGTH) {
        return false;
    }
    for (int i = 0; i < LINKLET_BSON_OID_BYTE_LENGTH; i++) {
        const int hi = linklet_bson_hex_value(hex[i * 2]);
        const int lo = linklet_bson_hex_value(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        oid->bytes[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

bool linklet_bson_oid_equal(const LinkletBsonOid *left, const LinkletBsonOid *right) {
    if (!left || !right) {
        return left == right;
    }
    return memcmp(left->bytes, right->bytes, sizeof(left->bytes)) == 0;
}

uint64_t linklet_bson_decimal128_high(const LinkletBsonDecimal128 *d) {
    if (!d) {
        return 0;
    }
    return linklet_bson_load_u64le(d->bytes + 8);
}

uint64_t linklet_bson_decimal128_low(const LinkletBsonDecimal128 *d) {
    if (!d) {
        return 0;
    }
    return linklet_bson_load_u64le(d->bytes);
}

void linklet_bson_decimal128_from_parts(LinkletBsonDecimal128 *d, uint64_t high, uint64_t low) {
    if (!d) {
        return;
    }
    linklet_bson_store_u64le(d->bytes, low);
    linklet_bson_store_u64le(d->bytes + 8, high);
}

const char *linklet_bson_type_name(LinkletBsonType type) {
    switch (type) {
    case LINKLET_BSON_TYPE_EOD:
        return "end of document";
    case LINKLET_BSON_TYPE_DOUBLE:
        return "double";
    case LINKLET_BSON_TYPE_UTF8:
        return "string";
    case LINKLET_BSON_TYPE_DOCUMENT:
        return "document";
    case LINKLET_BSON_TYPE_ARRAY:
        return "array";
    case LINKLET_BSON_TYPE_BINARY:
        return "binary";
    case LINKLET_BSON_TYPE_UNDEFINED:
        return "undefined";
    case LINKLET_BSON_TYPE_OID:
        return "objectId";
    case LINKLET_BSON_TYPE_BOOL:
        return "bool";
    case LINKLET_BSON_TYPE_DATE_TIME:
        return "dateTime";
    case LINKLET_BSON_TYPE_NULL:
        return "null";
    case LINKLET_BSON_TYPE_REGEX:
        return "regex";
    case LINKLET_BSON_TYPE_DBPOINTER:
        return "dbPointer";
    case LINKLET_BSON_TYPE_CODE:
        return "code";
    case LINKLET_BSON_TYPE_SYMBOL:
        return "symbol";
    case LINKLET_BSON_TYPE_CODEWSCOPE:
        return "codeWithScope";
    case LINKLET_BSON_TYPE_INT32:
        return "int32";
    case LINKLET_BSON_TYPE_TIMESTAMP:
        return "timestamp";
    case LINKLET_BSON_TYPE_INT64:
        return "int64";
    case LINKLET_BSON_TYPE_DECIMAL128:
        return "decimal128";
    case LINKLET_BSON_TYPE_MAXKEY:
        return "maxKey";
    case LINKLET_BSON_TYPE_MINKEY:
        return "minKey";
    default:
        return "unknown";
    }
}
