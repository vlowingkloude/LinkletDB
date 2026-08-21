#ifndef LINKLET_BSON_WRITER_H
#define LINKLET_BSON_WRITER_H

#include "bson.h"
#include "bson_reader.h"

bool linklet_bson_append_double(LinkletBson *bson, const char *key, double value,
                                LinkletBsonError *error);
bool linklet_bson_append_utf8(LinkletBson *bson, const char *key, const char *value, int length,
                              LinkletBsonError *error);
bool linklet_bson_append_document(LinkletBson *bson, const char *key, const LinkletBson *value,
                                  LinkletBsonError *error);
bool linklet_bson_append_array(LinkletBson *bson, const char *key, const LinkletBson *value,
                               LinkletBsonError *error);
bool linklet_bson_append_binary(LinkletBson *bson, const char *key, LinkletBsonSubtype subtype,
                                const uint8_t *data, size_t length, LinkletBsonError *error);
bool linklet_bson_append_oid(LinkletBson *bson, const char *key, const LinkletBsonOid *oid,
                             LinkletBsonError *error);
bool linklet_bson_append_bool(LinkletBson *bson, const char *key, bool value,
                              LinkletBsonError *error);
bool linklet_bson_append_date_time(LinkletBson *bson, const char *key, int64_t milliseconds,
                                   LinkletBsonError *error);
bool linklet_bson_append_null(LinkletBson *bson, const char *key, LinkletBsonError *error);
bool linklet_bson_append_undefined(LinkletBson *bson, const char *key, LinkletBsonError *error);
bool linklet_bson_append_regex(LinkletBson *bson, const char *key, const char *pattern,
                               const char *options, LinkletBsonError *error);
bool linklet_bson_append_dbpointer(LinkletBson *bson, const char *key, const char *collection,
                                   const LinkletBsonOid *oid, LinkletBsonError *error);
bool linklet_bson_append_code(LinkletBson *bson, const char *key, const char *javascript,
                              LinkletBsonError *error);
bool linklet_bson_append_symbol(LinkletBson *bson, const char *key, const char *value,
                                LinkletBsonError *error);
bool linklet_bson_append_code_with_scope(LinkletBson *bson, const char *key, const char *javascript,
                                         const LinkletBson *scope, LinkletBsonError *error);
bool linklet_bson_append_int32(LinkletBson *bson, const char *key, int32_t value,
                               LinkletBsonError *error);
bool linklet_bson_append_int64(LinkletBson *bson, const char *key, int64_t value,
                               LinkletBsonError *error);
bool linklet_bson_append_timestamp(LinkletBson *bson, const char *key, uint32_t increment,
                                   uint32_t timestamp, LinkletBsonError *error);
bool linklet_bson_append_decimal128(LinkletBson *bson, const char *key,
                                    const LinkletBsonDecimal128 *value, LinkletBsonError *error);
bool linklet_bson_append_maxkey(LinkletBson *bson, const char *key, LinkletBsonError *error);
bool linklet_bson_append_minkey(LinkletBson *bson, const char *key, LinkletBsonError *error);

bool linklet_bson_append_iterator(LinkletBson *bson, const LinkletBsonIterator *value,
                                  LinkletBsonError *error);

bool linklet_bson_merge(const LinkletBson *original, const LinkletBson *patch, LinkletBson *result,
                        LinkletBsonError *error);

bool linklet_bson_append_document_begin(LinkletBson *bson, const char *key, LinkletBson *child,
                                        LinkletBsonError *error);
bool linklet_bson_append_document_end(LinkletBson *bson, LinkletBson *child,
                                      LinkletBsonError *error);
bool linklet_bson_append_array_begin(LinkletBson *bson, const char *key, LinkletBson *child,
                                     LinkletBsonError *error);
bool linklet_bson_append_array_end(LinkletBson *bson, LinkletBson *child, LinkletBsonError *error);

bool linklet_bson_array_append_double(LinkletBson *array, double value, LinkletBsonError *error);
bool linklet_bson_array_append_utf8(LinkletBson *array, const char *value, int length,
                                    LinkletBsonError *error);
bool linklet_bson_array_append_document(LinkletBson *array, const LinkletBson *value,
                                        LinkletBsonError *error);
bool linklet_bson_array_append_bool(LinkletBson *array, bool value, LinkletBsonError *error);
bool linklet_bson_array_append_null(LinkletBson *array, LinkletBsonError *error);
bool linklet_bson_array_append_oid(LinkletBson *array, const LinkletBsonOid *oid,
                                   LinkletBsonError *error);
bool linklet_bson_array_append_int32(LinkletBson *array, int32_t value, LinkletBsonError *error);
bool linklet_bson_array_append_int64(LinkletBson *array, int64_t value, LinkletBsonError *error);
bool linklet_bson_array_append_date_time(LinkletBson *array, int64_t milliseconds,
                                         LinkletBsonError *error);

static inline bool linklet_bson_append_string(LinkletBson *bson, const char *key, const char *value,
                                              int length, LinkletBsonError *error) {
    return linklet_bson_append_utf8(bson, key, value, length, error);
}

#endif
