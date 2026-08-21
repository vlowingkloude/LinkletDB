#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wal.h"

enum {
    LINKLET_WAL_MAGIC_SIZE = 8,
    LINKLET_WAL_HEADER_SIZE = 64,
    LINKLET_WAL_VERSION = 1,
};

static const char WAL_MAGIC[LINKLET_WAL_MAGIC_SIZE] = {'L', 'N', 'K', 'W', 'A', 'L', '1', '\0'};

typedef struct LinkletWalHeader {
    char magic[LINKLET_WAL_MAGIC_SIZE];
    uint32_t version;
    unsigned char reserved[LINKLET_WAL_HEADER_SIZE - LINKLET_WAL_MAGIC_SIZE - sizeof(uint32_t)];
} LinkletWalHeader;

_Static_assert(sizeof(LinkletWalHeader) == LINKLET_WAL_HEADER_SIZE, "invalid WAL header size");

static void store_u32le(uint8_t *p, const uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t load_u32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void store_u64le(uint8_t *p, const uint64_t v) {
    store_u32le(p, (uint32_t)v);
    store_u32le(p + 4, (uint32_t)(v >> 32));
}

static uint64_t load_u64le(const uint8_t *p) {
    return (uint64_t)load_u32le(p) | ((uint64_t)load_u32le(p + 4) << 32);
}

LinkletMmapFile *linklet_wal_open(const char *path, const bool create, LinkletError *error) {
    LinkletMmapFile *wal =
        create ? linklet_mmap_create(path, error) : linklet_mmap_open(path, true, error);
    if (!wal) {
        return NULL;
    }

    if (create) {
        LinkletWalHeader header = {0};
        memcpy(header.magic, WAL_MAGIC, sizeof(WAL_MAGIC));
        header.version = LINKLET_WAL_VERSION;
        if (!linklet_mmap_resize(wal, LINKLET_WAL_HEADER_SIZE, error) ||
            !linklet_mmap_write_at(wal, &header, sizeof(header), 0, error) ||
            !linklet_mmap_flush(wal, error)) {
            linklet_mmap_close(wal);
            return NULL;
        }
    } else {
        LinkletWalHeader header;
        if (!linklet_mmap_read_at(wal, &header, sizeof(header), 0, error) ||
            memcmp(header.magic, WAL_MAGIC, sizeof(WAL_MAGIC)) != 0 ||
            header.version != LINKLET_WAL_VERSION) {
            if (error && !error->message[0]) {
                snprintf(error->message, sizeof(error->message),
                         "wal.ll has an incompatible header");
            }
            linklet_mmap_close(wal);
            return NULL;
        }
    }
    return wal;
}

void linklet_wal_close(LinkletMmapFile *wal) {
    linklet_mmap_close(wal);
}

bool linklet_wal_has_frame(const LinkletMmapFile *wal) {
    return wal && linklet_mmap_size(wal) > LINKLET_WAL_HEADER_SIZE;
}

static size_t record_size(const LinkletResolvedOperation *operation) {
    const size_t payload_size =
        operation->payload ? linklet_bson_get_length(operation->payload) : 0;
    return sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint64_t) +
           sizeof(uint8_t) + sizeof(uint32_t) + payload_size;
}

bool linklet_wal_write_frame(LinkletMmapFile *wal, const uint64_t transaction_id,
                             const LinkletResolvedOperation *operations,
                             const size_t operation_count, LinkletError *error) {
    size_t total = sizeof(uint64_t) + sizeof(uint64_t);
    for (size_t index = 0; index < operation_count; ++index) {
        total += record_size(&operations[index]);
    }
    uint8_t *buffer = (uint8_t *)malloc(total);
    if (!buffer) {
        if (error) {
            snprintf(error->message, sizeof(error->message), "out of memory writing WAL frame");
        }
        return false;
    }

    uint8_t *cursor = buffer;
    store_u64le(cursor, transaction_id);
    cursor += sizeof(uint64_t);
    store_u64le(cursor, (uint64_t)operation_count);
    cursor += sizeof(uint64_t);
    for (size_t index = 0; index < operation_count; ++index) {
        const LinkletResolvedOperation *operation = &operations[index];
        const size_t payload_size =
            operation->payload ? linklet_bson_get_length(operation->payload) : 0;
        store_u32le(cursor, (uint32_t)operation->operation);
        cursor += sizeof(uint32_t);
        store_u64le(cursor, operation->id);
        cursor += sizeof(uint64_t);
        store_u64le(cursor, operation->source_id);
        cursor += sizeof(uint64_t);
        store_u64le(cursor, operation->destination_id);
        cursor += sizeof(uint64_t);
        cursor[0] = operation->detach ? 1 : 0;
        cursor += sizeof(uint8_t);
        store_u32le(cursor, (uint32_t)payload_size);
        cursor += sizeof(uint32_t);
        if (payload_size != 0) {
            memcpy(cursor, linklet_bson_get_data(operation->payload), payload_size);
            cursor += payload_size;
        }
    }

    const bool ok = linklet_mmap_resize(wal, LINKLET_WAL_HEADER_SIZE + total, error) &&
                    linklet_mmap_write_at(wal, buffer, total, LINKLET_WAL_HEADER_SIZE, error) &&
                    linklet_mmap_flush(wal, error);
    free(buffer);
    return ok;
}

bool linklet_wal_apply(LinkletGraphStore *store, const LinkletResolvedOperation *operations,
                       const size_t operation_count, LinkletError *error) {
    for (size_t index = 0; index < operation_count; ++index) {
        const LinkletResolvedOperation *operation = &operations[index];
        switch (operation->operation) {
        case LINKLET_WAL_INSERT_NODE: {
            if (linklet_graph_store_node_exists(store, operation->id)) {
                break;
            }
            uint64_t assigned = 0;
            if (!linklet_graph_store_insert_node(store, operation->payload, &assigned, error)) {
                return false;
            }
            if (assigned != operation->id) {
                if (error) {
                    snprintf(error->message, sizeof(error->message),
                             "WAL replay assigned node id %llu, expected %llu",
                             (unsigned long long)assigned, (unsigned long long)operation->id);
                }
                return false;
            }
            break;
        }
        case LINKLET_WAL_INSERT_EDGE: {
            if (linklet_graph_store_edge_exists(store, operation->id)) {
                break;
            }
            uint64_t assigned = 0;
            if (!linklet_graph_store_insert_edge(store, operation->source_id,
                                                 operation->destination_id, operation->payload,
                                                 &assigned, error)) {
                return false;
            }
            if (assigned != operation->id) {
                if (error) {
                    snprintf(error->message, sizeof(error->message),
                             "WAL replay assigned edge id %llu, expected %llu",
                             (unsigned long long)assigned, (unsigned long long)operation->id);
                }
                return false;
            }
            break;
        }
        case LINKLET_WAL_UPDATE_NODE:
            if (!linklet_graph_store_node_exists(store, operation->id)) {
                break;
            }
            if (!linklet_graph_store_update_node(store, operation->id, operation->payload, error)) {
                return false;
            }
            break;
        case LINKLET_WAL_UPDATE_EDGE:
            if (!linklet_graph_store_edge_exists(store, operation->id)) {
                break;
            }
            if (!linklet_graph_store_update_edge(store, operation->id, operation->payload, error)) {
                return false;
            }
            break;
        case LINKLET_WAL_DELETE_NODE:
            if (!linklet_graph_store_node_exists(store, operation->id)) {
                break;
            }
            if (!linklet_graph_store_delete_node(store, operation->id, operation->detach, error)) {
                return false;
            }
            break;
        case LINKLET_WAL_DELETE_EDGE:
            if (!linklet_graph_store_edge_exists(store, operation->id)) {
                break;
            }
            if (!linklet_graph_store_delete_edge(store, operation->id, error)) {
                return false;
            }
            break;
        default:
            if (error) {
                snprintf(error->message, sizeof(error->message), "unknown WAL opcode");
            }
            return false;
        }
    }
    return true;
}

bool linklet_wal_clear(LinkletMmapFile *wal, LinkletError *error) {
    return linklet_mmap_resize(wal, LINKLET_WAL_HEADER_SIZE, error) &&
           linklet_mmap_flush(wal, error);
}

bool linklet_wal_replay(LinkletMmapFile *wal, LinkletGraphStore *store, LinkletError *error) {
    const uint64_t size = linklet_mmap_size(wal);
    if (size <= LINKLET_WAL_HEADER_SIZE) {
        return true;
    }

    const size_t frame_size = (size_t)(size - LINKLET_WAL_HEADER_SIZE);
    uint8_t *buffer = (uint8_t *)malloc(frame_size);
    if (!buffer || !linklet_mmap_read_at(wal, buffer, frame_size, LINKLET_WAL_HEADER_SIZE, error)) {
        free(buffer);
        return false;
    }

    const uint8_t *cursor = buffer + sizeof(uint64_t);
    const uint64_t operation_count = load_u64le(cursor);
    cursor += sizeof(uint64_t);

    LinkletResolvedOperation *operations =
        (LinkletResolvedOperation *)calloc((size_t)operation_count, sizeof(*operations));
    LinkletBson *views = (LinkletBson *)calloc((size_t)operation_count, sizeof(*views));
    bool ok = operations != NULL && views != NULL;
    if (!ok && error) {
        snprintf(error->message, sizeof(error->message), "out of memory replaying WAL");
    }

    for (uint64_t index = 0; ok && index < operation_count; ++index) {
        LinkletResolvedOperation *operation = &operations[index];
        operation->operation = (LinkletWalOperation)load_u32le(cursor);
        cursor += sizeof(uint32_t);
        operation->id = load_u64le(cursor);
        cursor += sizeof(uint64_t);
        operation->source_id = load_u64le(cursor);
        cursor += sizeof(uint64_t);
        operation->destination_id = load_u64le(cursor);
        cursor += sizeof(uint64_t);
        operation->detach = cursor[0] != 0;
        cursor += sizeof(uint8_t);
        const uint32_t payload_size = load_u32le(cursor);
        cursor += sizeof(uint32_t);
        if (payload_size != 0) {
            views[index] = linklet_bson_view(cursor, payload_size);
            operation->payload = &views[index];
            cursor += payload_size;
        }
    }

    if (ok) {
        ok = linklet_wal_apply(store, operations, (size_t)operation_count, error);
    }
    if (ok) {
        ok = linklet_wal_clear(wal, error);
    }

    free(views);
    free(operations);
    free(buffer);
    return ok;
}
