#ifndef LINKLET_BSON_READER_H
#define LINKLET_BSON_READER_H

#include "bson.h"

typedef struct LinkletBsonIterator {
    const uint8_t *data;
    size_t length;
    size_t offset;
    size_t next_offset;
    LinkletBsonType type;
    const char *key;
    size_t key_length;
    const uint8_t *value;
} LinkletBsonIterator;

bool linklet_bson_iterator_init(LinkletBsonIterator *iterator, const LinkletBson *bson);
bool linklet_bson_iterator_init_from_data(LinkletBsonIterator *iterator, const uint8_t *data,
                                          size_t length);
bool linklet_bson_iterator_next(LinkletBsonIterator *iterator);

const char *linklet_bson_iterator_key(const LinkletBsonIterator *iterator);
size_t linklet_bson_iterator_key_length(const LinkletBsonIterator *iterator);
LinkletBsonType linklet_bson_iterator_type(const LinkletBsonIterator *iterator);

bool linklet_bson_iterator_double(const LinkletBsonIterator *iterator, double *output);
bool linklet_bson_iterator_utf8(const LinkletBsonIterator *iterator, const char **string,
                                size_t *length);
bool linklet_bson_iterator_document(const LinkletBsonIterator *iterator, const uint8_t **data,
                                    size_t *length);
bool linklet_bson_iterator_array(const LinkletBsonIterator *iterator, const uint8_t **data,
                                 size_t *length);
bool linklet_bson_iterator_binary(const LinkletBsonIterator *iterator, LinkletBsonSubtype *subtype,
                                  const uint8_t **data, size_t *length);
bool linklet_bson_iterator_oid(const LinkletBsonIterator *iterator, LinkletBsonOid *output);
bool linklet_bson_iterator_bool(const LinkletBsonIterator *iterator, bool *output);
bool linklet_bson_iterator_date_time(const LinkletBsonIterator *iterator, int64_t *milliseconds);
bool linklet_bson_iterator_regex(const LinkletBsonIterator *iterator, const char **pattern,
                                 const char **options);
bool linklet_bson_iterator_dbpointer(const LinkletBsonIterator *iterator, const char **collection,
                                     LinkletBsonOid *oid);
bool linklet_bson_iterator_code(const LinkletBsonIterator *iterator, const char **code,
                                size_t *length);
bool linklet_bson_iterator_symbol(const LinkletBsonIterator *iterator, const char **symbol,
                                  size_t *length);
bool linklet_bson_iterator_code_with_scope(const LinkletBsonIterator *iterator, const char **code,
                                           size_t *code_length, const uint8_t **scope,
                                           size_t *scope_length);
bool linklet_bson_iterator_int32(const LinkletBsonIterator *iterator, int32_t *output);
bool linklet_bson_iterator_int64(const LinkletBsonIterator *iterator, int64_t *output);
bool linklet_bson_iterator_timestamp(const LinkletBsonIterator *iterator, uint32_t *increment,
                                     uint32_t *timestamp);
bool linklet_bson_iterator_decimal128(const LinkletBsonIterator *iterator,
                                      LinkletBsonDecimal128 *output);

bool linklet_bson_find(const LinkletBson *bson, const char *key, LinkletBsonIterator *iterator);

bool linklet_bson_has_field(const LinkletBson *bson, const char *key);
bool linklet_bson_get_double(const LinkletBson *bson, const char *key, double *output);
bool linklet_bson_get_utf8(const LinkletBson *bson, const char *key, const char **string,
                           size_t *length);
bool linklet_bson_get_document(const LinkletBson *bson, const char *key, const uint8_t **data,
                               size_t *length);
bool linklet_bson_get_array(const LinkletBson *bson, const char *key, const uint8_t **data,
                            size_t *length);
bool linklet_bson_get_bool(const LinkletBson *bson, const char *key, bool *output);
bool linklet_bson_get_date_time(const LinkletBson *bson, const char *key, int64_t *milliseconds);
bool linklet_bson_get_oid(const LinkletBson *bson, const char *key, LinkletBsonOid *output);
bool linklet_bson_get_int32(const LinkletBson *bson, const char *key, int32_t *output);
bool linklet_bson_get_int64(const LinkletBson *bson, const char *key, int64_t *output);
bool linklet_bson_get_timestamp(const LinkletBson *bson, const char *key, uint32_t *increment,
                                uint32_t *timestamp);

size_t linklet_bson_count_keys(const LinkletBson *bson);

static inline bool linklet_bson_iterator_string(const LinkletBsonIterator *iterator,
                                                const char **string, size_t *length) {
    return linklet_bson_iterator_utf8(iterator, string, length);
}
static inline bool linklet_bson_get_string(const LinkletBson *bson, const char *key,
                                           const char **string, size_t *length) {
    return linklet_bson_get_utf8(bson, key, string, length);
}
#endif
