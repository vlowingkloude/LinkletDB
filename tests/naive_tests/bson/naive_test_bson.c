#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "naive_tests.h"
#include "bson.h"
#include "bson_reader.h"
#include "bson_writer.h"

bool test_bson_scalar_and_nested_roundtrip(void) {
    LinkletBsonError err;
    LinkletBson doc;
    linklet_bson_init(&doc);

    LinkletBsonOid oid;
    linklet_bson_oid_init(&oid);

    const uint8_t blob[] = {0xde, 0xad, 0xbe, 0xef};

    bool ok = linklet_bson_append_utf8(&doc, "name", "Alice", -1, &err) &&
              linklet_bson_append_int32(&doc, "age", 30, &err) &&
              linklet_bson_append_double(&doc, "score", 3.5, &err) &&
              linklet_bson_append_bool(&doc, "active", true, &err) &&
              linklet_bson_append_null(&doc, "nothing", &err) &&
              linklet_bson_append_int64(&doc, "big", INT64_C(9007199254740993), &err) &&
              linklet_bson_append_date_time(&doc, "ts", INT64_C(1700000000123), &err) &&
              linklet_bson_append_oid(&doc, "id", &oid, &err) &&
              linklet_bson_append_binary(&doc, "blob", LINKLET_BSON_SUBTYPE_BINARY, blob,
                                         sizeof(blob), &err);
    if (!ok) {
        fprintf(stderr, "  scalar append failed: %s\n", err.message);
        linklet_bson_destroy(&doc);
        return false;
    }

    LinkletBson child;
    ok = linklet_bson_append_document_begin(&doc, "address", &child, &err) &&
         linklet_bson_append_utf8(&child, "city", "Beijing", -1, &err) &&
         linklet_bson_append_document_end(&doc, &child, &err);
    if (!ok) {
        fprintf(stderr, "  nested document failed: %s\n", err.message);
        linklet_bson_destroy(&doc);
        return false;
    }

    LinkletBson arr;
    ok = linklet_bson_append_array_begin(&doc, "tags", &arr, &err) &&
         linklet_bson_array_append_utf8(&arr, "gql", -1, &err) &&
         linklet_bson_array_append_int32(&arr, 7, &err) &&
         linklet_bson_append_array_end(&doc, &arr, &err);
    if (!ok) {
        fprintf(stderr, "  array append failed: %s\n", err.message);
        linklet_bson_destroy(&doc);
        return false;
    }

    if (!linklet_bson_validate_document(&doc, &err)) {
        fprintf(stderr, "  validate failed: %s\n", err.message);
        linklet_bson_destroy(&doc);
        return false;
    }
    if (linklet_bson_count_keys(&doc) != 11) {
        fprintf(stderr, "  expected 11 keys, got %zu\n", linklet_bson_count_keys(&doc));
        linklet_bson_destroy(&doc);
        return false;
    }

    int32_t age = 0;
    double score = 0;
    bool active = false;
    int64_t big = 0, ts = 0;
    const char *name = NULL;
    size_t name_len = 0;
    LinkletBsonOid oid2;
    const uint8_t *bin = NULL;
    size_t bin_len = 0;
    LinkletBsonSubtype subtype = LINKLET_BSON_SUBTYPE_BINARY;

    ok = linklet_bson_get_utf8(&doc, "name", &name, &name_len) &&
         linklet_bson_get_int32(&doc, "age", &age) &&
         linklet_bson_get_double(&doc, "score", &score) &&
         linklet_bson_get_bool(&doc, "active", &active) &&
         linklet_bson_get_int64(&doc, "big", &big) && linklet_bson_get_date_time(&doc, "ts", &ts) &&
         linklet_bson_get_oid(&doc, "id", &oid2);
    if (!ok || name_len != 5 || memcmp(name, "Alice", 5) != 0 || age != 30 || score != 3.5 ||
        !active || big != INT64_C(9007199254740993) || ts != INT64_C(1700000000123) ||
        !linklet_bson_oid_equal(&oid, &oid2)) {
        fprintf(stderr, "  scalar read-back mismatch\n");
        linklet_bson_destroy(&doc);
        return false;
    }

    LinkletBsonIterator iterator;
    if (!linklet_bson_find(&doc, "blob", &iterator) ||
        !linklet_bson_iterator_binary(&iterator, &subtype, &bin, &bin_len) ||
        subtype != LINKLET_BSON_SUBTYPE_BINARY || bin_len != sizeof(blob) ||
        memcmp(bin, blob, sizeof(blob)) != 0) {
        fprintf(stderr, "  binary read-back mismatch\n");
        linklet_bson_destroy(&doc);
        return false;
    }

    const uint8_t *addr = NULL;
    size_t addr_len = 0;
    if (!linklet_bson_get_document(&doc, "address", &addr, &addr_len)) {
        fprintf(stderr, "  missing nested document\n");
        linklet_bson_destroy(&doc);
        return false;
    }
    LinkletBson addr_view = linklet_bson_view(addr, addr_len);
    const char *city = NULL;
    if (!linklet_bson_get_utf8(&addr_view, "city", &city, NULL) || strcmp(city, "Beijing") != 0) {
        fprintf(stderr, "  nested document read-back mismatch\n");
        linklet_bson_destroy(&doc);
        return false;
    }

    const uint8_t *tags = NULL;
    size_t tags_len = 0;
    if (!linklet_bson_get_array(&doc, "tags", &tags, &tags_len)) {
        fprintf(stderr, "  missing array\n");
        linklet_bson_destroy(&doc);
        return false;
    }
    LinkletBson tags_view = linklet_bson_view(tags, tags_len);
    if (linklet_bson_count_keys(&tags_view) != 2) {
        fprintf(stderr, "  expected 2 array elements\n");
        linklet_bson_destroy(&doc);
        return false;
    }
    const char *tag0 = NULL;
    if (!linklet_bson_get_utf8(&tags_view, "0", &tag0, NULL) || strcmp(tag0, "gql") != 0) {
        fprintf(stderr, "  array element 0 mismatch\n");
        linklet_bson_destroy(&doc);
        return false;
    }

    LinkletBson *copy = linklet_bson_copy(&doc);
    if (!copy || copy->length != doc.length ||
        memcmp(linklet_bson_get_data(copy), linklet_bson_get_data(&doc), doc.length) != 0) {
        fprintf(stderr, "  linklet_bson_copy mismatch\n");
        linklet_bson_free(copy);
        linklet_bson_destroy(&doc);
        return false;
    }
    linklet_bson_free(copy);
    linklet_bson_destroy(&doc);
    return true;
}

bool test_bson_validation_rejects_malformed(void) {
    LinkletBsonError err;
    LinkletBson doc;
    linklet_bson_init(&doc);

    const char bad_utf8[] = {(char)0xFF, (char)0xFE, 0};
    if (linklet_bson_append_utf8(&doc, "s", bad_utf8, 2, &err)) {
        fprintf(stderr, "  invalid UTF-8 string was accepted\n");
        linklet_bson_destroy(&doc);
        return false;
    }

    if (linklet_bson_append_int32(&doc, "", 1, &err)) {
        fprintf(stderr, "  empty key was accepted\n");
        linklet_bson_destroy(&doc);
        return false;
    }
    linklet_bson_append_int32(&doc, "k", 1, &err);
    linklet_bson_destroy(&doc);

    LinkletBson arr;
    linklet_bson_init(&arr);
    linklet_bson_append_int32(&arr, "0", 1, &err);
    linklet_bson_append_int32(&arr, "2", 2, &err);
    LinkletBson outer;
    linklet_bson_init(&outer);
    linklet_bson_append_array(&outer, "a", &arr, &err);
    if (linklet_bson_validate_document(&outer, &err)) {
        fprintf(stderr, "  non-sequential array was accepted\n");
        linklet_bson_destroy(&outer);
        linklet_bson_destroy(&arr);
        return false;
    }
    linklet_bson_destroy(&outer);
    linklet_bson_destroy(&arr);

    LinkletBson good;
    linklet_bson_init(&good);
    linklet_bson_append_int32(&good, "x", 42, &err);
    uint8_t *bytes = (uint8_t *)malloc(good.length);
    memcpy(bytes, linklet_bson_get_data(&good), good.length);
    bytes[0] = (uint8_t)(good.length + 1);
    if (linklet_bson_validate(bytes, good.length, &err)) {
        fprintf(stderr, "  corrupted length prefix was accepted\n");
        free(bytes);
        linklet_bson_destroy(&good);
        return false;
    }
    free(bytes);

    if (linklet_bson_validate(linklet_bson_get_data(&good), good.length - 1, &err)) {
        fprintf(stderr, "  truncated buffer was accepted\n");
        linklet_bson_destroy(&good);
        return false;
    }
    linklet_bson_destroy(&good);

    static const uint8_t raw_bad_utf8[] = {0x0E, 0x00, 0x00, 0x00, 0x02, 'a',  0x00,
                                           0x02, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00};
    if (linklet_bson_validate(raw_bad_utf8, sizeof(raw_bad_utf8), &err)) {
        fprintf(stderr, "  raw invalid UTF-8 document was accepted\n");
        return false;
    }

    static const uint8_t raw_bad_type[] = {0x0B, 0x00, 0x00, 0x00, 0x20, 'a',
                                           0x00, 0x01, 0x02, 0x03, 0x04, 0x00};
    if (linklet_bson_validate(raw_bad_type, sizeof(raw_bad_type), &err)) {
        fprintf(stderr, "  unknown element type was accepted\n");
        return false;
    }

    return true;
}

bool test_bson_oid_and_timestamp_roundtrip(void) {
    LinkletBsonOid oid;
    linklet_bson_oid_init_from_time(&oid, 1700000000u);
    if (linklet_bson_oid_time(&oid) != 1700000000u) {
        fprintf(stderr, "  oid timestamp mismatch\n");
        return false;
    }
    char hex[25];
    linklet_bson_oid_to_string(&oid, hex);
    if (strlen(hex) != 24) {
        fprintf(stderr, "  oid hex length mismatch\n");
        return false;
    }
    LinkletBsonOid oid2;
    if (!linklet_bson_oid_from_string(hex, &oid2) || !linklet_bson_oid_equal(&oid, &oid2)) {
        fprintf(stderr, "  oid string roundtrip mismatch\n");
        return false;
    }
    if (linklet_bson_oid_from_string("zz", &oid2)) {
        fprintf(stderr, "  invalid oid string was accepted\n");
        return false;
    }

    LinkletBsonError err;
    LinkletBson doc;
    linklet_bson_init(&doc);
    if (!linklet_bson_append_timestamp(&doc, "t", 5u, 1700000000u, &err) ||
        !linklet_bson_validate_document(&doc, &err)) {
        fprintf(stderr, "  timestamp append/validate failed: %s\n", err.message);
        linklet_bson_destroy(&doc);
        return false;
    }
    uint32_t increment = 0, timestamp = 0;
    if (!linklet_bson_get_timestamp(&doc, "t", &increment, &timestamp) || increment != 5u ||
        timestamp != 1700000000u) {
        fprintf(stderr, "  timestamp read-back mismatch\n");
        linklet_bson_destroy(&doc);
        return false;
    }
    linklet_bson_destroy(&doc);
    return true;
}
