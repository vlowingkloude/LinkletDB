#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"
#include "binder.h"
#include "bson_reader.h"
#include "gql_parser.h"
#include "mmap_file.h"
#include "wal.h"

static char *db_file_path(const char *path, const char *name) {
    const size_t size = strlen(path) + 1 + strlen(name) + 1;
    char *buffer = (char *)malloc(size);
    if (buffer) {
        snprintf(buffer, size, "%s/%s", path, name);
    }
    return buffer;
}

static uint32_t load_u32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool write_metadata(const char *path, const LinkletCatalog *catalog, const bool create_file,
                           LinkletError *error) {
    LinkletBson doc;
    linklet_bson_init(&doc);
    if (!linklet_catalog_to_bson(catalog, &doc, error)) {
        return false;
    }

    char *metadata_path = db_file_path(path, "metadata.ll");
    if (!metadata_path) {
        if (error) {
            snprintf(error->message, sizeof(error->message), "out of memory");
        }
        linklet_bson_destroy(&doc);
        return false;
    }
    LinkletMmapFile *file = create_file ? linklet_mmap_create(metadata_path, error)
                                        : linklet_mmap_open(metadata_path, true, error);
    free(metadata_path);
    if (!file) {
        linklet_bson_destroy(&doc);
        return false;
    }

    const size_t length = linklet_bson_get_length(&doc);
    const uint8_t *data = linklet_bson_get_data(&doc);
    const bool ok = linklet_mmap_resize(file, length, error) &&
                    linklet_mmap_write_at(file, data, length, 0, error) &&
                    linklet_mmap_flush(file, error);
    linklet_mmap_close(file);
    linklet_bson_destroy(&doc);
    return ok;
}

static bool read_metadata(const char *path, LinkletCatalog *catalog, LinkletError *error) {
    char *metadata_path = db_file_path(path, "metadata.ll");
    if (!metadata_path) {
        if (error) {
            snprintf(error->message, sizeof(error->message), "out of memory");
        }
        return false;
    }
    LinkletMmapFile *file = linklet_mmap_open(metadata_path, true, error);
    free(metadata_path);
    if (!file) {
        return false;
    }

    uint8_t header[sizeof(uint32_t)];
    bool ok = linklet_mmap_read_at(file, header, sizeof(header), 0, error);
    uint32_t size = 0;
    if (ok) {
        size = load_u32le(header);
        if (size < LINKLET_BSON_EMPTY_DOCUMENT_SIZE) {
            if (error) {
                snprintf(error->message, sizeof(error->message), "metadata.ll is corrupt");
            }
            ok = false;
        }
    }
    uint8_t *bytes = NULL;
    if (ok) {
        bytes = (uint8_t *)malloc(size);
        ok = bytes != NULL && linklet_mmap_read_at(file, bytes, size, 0, error);
    }
    LinkletBsonError bson_error;
    if (ok) {
        ok = linklet_bson_validate(bytes, size, &bson_error);
        if (!ok && error) {
            snprintf(error->message, sizeof(error->message), "metadata.ll: %.200s",
                     bson_error.message);
        }
    }
    if (ok) {
        const LinkletBson doc = linklet_bson_view(bytes, size);
        ok = linklet_catalog_from_bson(catalog, &doc, error);
    }
    free(bytes);
    linklet_mmap_close(file);
    return ok;
}

LinkletDatabase *linklet_database_create(const char *path, const char *graph_name,
                                         const size_t initial_edge_capacity, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!path || !path[0]) {
        if (error) {
            snprintf(error->message, sizeof(error->message), "database path is required");
        }
        return NULL;
    }
    LinkletDatabase *database = (LinkletDatabase *)calloc(1, sizeof(*database));
    if (!database) {
        if (error) {
            snprintf(error->message, sizeof(error->message), "could not allocate database");
        }
        return NULL;
    }
    database->store = linklet_graph_store_create(path, initial_edge_capacity, error);
    if (!database->store) {
        free(database);
        return NULL;
    }
    database->path = strdup(path);
    linklet_catalog_init(&database->catalog);
    database->catalog.name = strdup(graph_name ? graph_name : "");
    if (!database->path || !database->catalog.name ||
        !write_metadata(path, &database->catalog, true, error)) {
        linklet_catalog_destroy(&database->catalog);
        linklet_graph_store_close(database->store, NULL);
        free(database->path);
        free(database);
        return NULL;
    }
    char *wal_file = db_file_path(path, "wal.ll");
    LinkletMmapFile *wal = wal_file ? linklet_wal_open(wal_file, true, error) : NULL;
    free(wal_file);
    if (!wal) {
        linklet_catalog_destroy(&database->catalog);
        linklet_graph_store_close(database->store, NULL);
        free(database->path);
        free(database);
        return NULL;
    }
    linklet_wal_close(wal);
    return database;
}

LinkletDatabase *linklet_database_open(const char *path, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!path || !path[0]) {
        if (error) {
            snprintf(error->message, sizeof(error->message), "database path is required");
        }
        return NULL;
    }
    LinkletDatabase *database = (LinkletDatabase *)calloc(1, sizeof(*database));
    if (!database) {
        if (error) {
            snprintf(error->message, sizeof(error->message), "could not allocate database");
        }
        return NULL;
    }
    database->store = linklet_graph_store_open(path, error);
    if (!database->store) {
        free(database);
        return NULL;
    }
    database->path = strdup(path);
    linklet_catalog_init(&database->catalog);
    if (!database->path || !read_metadata(path, &database->catalog, error)) {
        linklet_catalog_destroy(&database->catalog);
        linklet_graph_store_close(database->store, NULL);
        free(database->path);
        free(database);
        return NULL;
    }
    char *wal_file = db_file_path(path, "wal.ll");
    LinkletMmapFile *wal = wal_file ? linklet_wal_open(wal_file, false, error) : NULL;
    free(wal_file);
    if (!wal || !linklet_wal_replay(wal, database->store, error)) {
        if (wal) {
            linklet_wal_close(wal);
        }
        linklet_catalog_destroy(&database->catalog);
        linklet_graph_store_close(database->store, NULL);
        free(database->path);
        free(database);
        return NULL;
    }
    linklet_wal_close(wal);
    return database;
}

bool linklet_database_close(LinkletDatabase *database, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!database) {
        return true;
    }
    const bool success = linklet_graph_store_close(database->store, error);
    linklet_catalog_destroy(&database->catalog);
    free(database->path);
    free(database);
    return success;
}

bool linklet_database_execute(LinkletDatabase *database, const char *gql, LinkletResult *result,
                              LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!database || !gql || !result) {
        if (error) {
            snprintf(error->message, sizeof(error->message),
                     "database, GQL text, and result are required");
        }
        return false;
    }

    GqlParseError parse_error;
    GqlNode *ast = gql_parse_program(gql, &parse_error);
    if (!ast) {
        if (error) {
            snprintf(error->message, sizeof(error->message), "parse error at %zu:%zu: %.180s",
                     parse_error.line, parse_error.column, parse_error.message);
        }
        return false;
    }

    *result = (LinkletResult){0};
    bool success;
    if (linklet_is_catalog_ddl(ast)) {
        success = linklet_catalog_apply_ddl(&database->catalog, ast, error) &&
                  write_metadata(database->path, &database->catalog, false, error);
        if (success) {
            result->kind = LINKLET_RESULT_COUNT;
            result->affected_count = 1;
        }
    } else {
        LinkletLogicalPlan plan = {.calls = NULL, .call_count = 0};
        success = linklet_bind(ast, &plan, error);
        if (success) {
            success = linklet_execute(&plan, database->store, result, error);
        }
        linklet_logical_plan_destroy(&plan);
    }

    gql_node_free(ast);
    return success;
}

const LinkletCatalog *linklet_database_catalog(const LinkletDatabase *database) {
    return database ? &database->catalog : NULL;
}
