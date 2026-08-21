#ifndef LINKLET_BSON_H
#define LINKLET_BSON_H

#include <stdbool.h>
#include <stdint.h>

#define LINKLET_BSON_VERSION_MAJOR 0
#define LINKLET_BSON_VERSION_MINOR 1

#define LINKLET_BSON_DEFAULT_MAX_DEPTH 200

enum {
    LINKLET_BSON_ERROR_MESSAGE_CAPACITY = 256,
    LINKLET_BSON_EMPTY_DOCUMENT_SIZE = 5,
    LINKLET_BSON_OID_BYTE_LENGTH = 12,
    LINKLET_BSON_OID_STRING_LENGTH = 24,
    LINKLET_BSON_OID_STRING_CAPACITY = LINKLET_BSON_OID_STRING_LENGTH + 1,
    LINKLET_BSON_DECIMAL128_BYTE_LENGTH = 16,
};

typedef enum LinkletBsonType {
    LINKLET_BSON_TYPE_EOD = 0x00,
    LINKLET_BSON_TYPE_DOUBLE = 0x01,
    LINKLET_BSON_TYPE_UTF8 = 0x02,
    LINKLET_BSON_TYPE_DOCUMENT = 0x03,
    LINKLET_BSON_TYPE_ARRAY = 0x04,
    LINKLET_BSON_TYPE_BINARY = 0x05,
    LINKLET_BSON_TYPE_UNDEFINED = 0x06,
    LINKLET_BSON_TYPE_OID = 0x07,
    LINKLET_BSON_TYPE_BOOL = 0x08,
    LINKLET_BSON_TYPE_DATE_TIME = 0x09,
    LINKLET_BSON_TYPE_NULL = 0x0A,
    LINKLET_BSON_TYPE_REGEX = 0x0B,
    LINKLET_BSON_TYPE_DBPOINTER = 0x0C,
    LINKLET_BSON_TYPE_CODE = 0x0D,
    LINKLET_BSON_TYPE_SYMBOL = 0x0E,
    LINKLET_BSON_TYPE_CODEWSCOPE = 0x0F,
    LINKLET_BSON_TYPE_INT32 = 0x10,
    LINKLET_BSON_TYPE_TIMESTAMP = 0x11,
    LINKLET_BSON_TYPE_INT64 = 0x12,
    LINKLET_BSON_TYPE_DECIMAL128 = 0x13,
    LINKLET_BSON_TYPE_MAXKEY = 0x7F,
    LINKLET_BSON_TYPE_MINKEY = 0xFF
} LinkletBsonType;

typedef enum LinkletBsonSubtype {
    LINKLET_BSON_SUBTYPE_BINARY = 0x00,
    LINKLET_BSON_SUBTYPE_FUNCTION = 0x01,
    LINKLET_BSON_SUBTYPE_BINARY_OLD = 0x02,
    LINKLET_BSON_SUBTYPE_UUID_OLD = 0x03,
    LINKLET_BSON_SUBTYPE_UUID = 0x04,
    LINKLET_BSON_SUBTYPE_MD5 = 0x05,
    LINKLET_BSON_SUBTYPE_ENCRYPTED = 0x06,
    LINKLET_BSON_SUBTYPE_COLUMN = 0x07,
    LINKLET_BSON_SUBTYPE_SENSITIVE = 0x08,
    LINKLET_BSON_SUBTYPE_VECTOR = 0x09,
    LINKLET_BSON_SUBTYPE_USER = 0x80
} LinkletBsonSubtype;

typedef struct LinkletBsonOid {
    uint8_t bytes[LINKLET_BSON_OID_BYTE_LENGTH];
} LinkletBsonOid;

typedef struct LinkletBsonDecimal128 {
    uint8_t bytes[LINKLET_BSON_DECIMAL128_BYTE_LENGTH];
} LinkletBsonDecimal128;

typedef struct LinkletBsonError {
    char message[LINKLET_BSON_ERROR_MESSAGE_CAPACITY];
} LinkletBsonError;

enum {
    LINKLET_BSON_FLAG_NONE = 0,
    LINKLET_BSON_FLAG_STATIC = 1u << 0
};

typedef struct LinkletBson {
    uint8_t *data;
    size_t length;
    size_t capacity;
    uint32_t flags;
} LinkletBson;

void linklet_bson_init(LinkletBson *bson);
void linklet_bson_init_static(LinkletBson *bson, const uint8_t *data, size_t length);
void linklet_bson_destroy(LinkletBson *bson);

void linklet_bson_free(LinkletBson *bson);
void linklet_bson_clear(LinkletBson *bson);

LinkletBson *linklet_bson_new(void);
LinkletBson *linklet_bson_new_from_data(const uint8_t *data, size_t length);
LinkletBson *linklet_bson_copy(const LinkletBson *bson);

LinkletBson linklet_bson_view(const uint8_t *data, size_t length);

const uint8_t *linklet_bson_get_data(const LinkletBson *bson);
size_t linklet_bson_get_length(const LinkletBson *bson);
bool linklet_bson_empty(const LinkletBson *bson);

bool linklet_bson_validate(const uint8_t *data, size_t length, LinkletBsonError *error);
bool linklet_bson_validate_document(const LinkletBson *bson, LinkletBsonError *error);

void linklet_bson_oid_init(LinkletBsonOid *oid);
void linklet_bson_oid_init_from_time(LinkletBsonOid *oid, uint32_t time_sec);
uint32_t linklet_bson_oid_time(const LinkletBsonOid *oid);
bool linklet_bson_oid_from_string(const char *hex, LinkletBsonOid *oid);
void linklet_bson_oid_to_string(const LinkletBsonOid *oid,
                                char output[LINKLET_BSON_OID_STRING_CAPACITY]);
bool linklet_bson_oid_equal(const LinkletBsonOid *a, const LinkletBsonOid *bson);

uint64_t linklet_bson_decimal128_high(const LinkletBsonDecimal128 *d);
uint64_t linklet_bson_decimal128_low(const LinkletBsonDecimal128 *d);
void linklet_bson_decimal128_from_parts(LinkletBsonDecimal128 *d, uint64_t high, uint64_t low);

const char *linklet_bson_type_name(LinkletBsonType type);

#endif
