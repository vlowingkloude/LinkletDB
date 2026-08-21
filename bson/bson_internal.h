#ifndef LINKLET_BSON_INTERNAL_H
#define LINKLET_BSON_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bson.h"

void linklet_bson_set_error(LinkletBsonError *error, const char *format, ...);

bool linklet_bson_utf8_validate(const uint8_t *data, size_t length);

size_t linklet_bson_value_size(LinkletBsonType type, const uint8_t *value, const uint8_t *end);

const uint8_t *linklet_bson_empty_doc_bytes(size_t *length);

static inline void linklet_bson_store_u32le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static inline uint32_t linklet_bson_load_u32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline void linklet_bson_store_u64le(uint8_t *p, uint64_t v) {
    linklet_bson_store_u32le(p, (uint32_t)(v));
    linklet_bson_store_u32le(p + 4, (uint32_t)(v >> 32));
}

static inline uint64_t linklet_bson_load_u64le(const uint8_t *p) {
    return (uint64_t)linklet_bson_load_u32le(p) | ((uint64_t)linklet_bson_load_u32le(p + 4) << 32);
}

static inline int32_t linklet_bson_load_i32le(const uint8_t *p) {
    return (int32_t)linklet_bson_load_u32le(p);
}

static inline int64_t linklet_bson_load_i64le(const uint8_t *p) {
    return (int64_t)linklet_bson_load_u64le(p);
}

static inline void linklet_bson_store_double_le(uint8_t *p, double v) {
    uint64_t bits;
    memcpy(&bits, &v, sizeof(bits));
    linklet_bson_store_u64le(p, bits);
}

static inline double linklet_bson_load_double_le(const uint8_t *p) {
    uint64_t bits = linklet_bson_load_u64le(p);
    double v;
    memcpy(&v, &bits, sizeof(v));
    return v;
}

static inline bool linklet_bson_bytes_available(const uint8_t *value, const uint8_t *end,
                                                size_t n) {
    return value != NULL && end != NULL && value <= end && (size_t)(end - value) >= n;
}

#endif
