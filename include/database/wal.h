#ifndef LINKLET_WAL_H
#define LINKLET_WAL_H

#include <stdbool.h>
#include <stdint.h>

#include "graph_store.h"
#include "linklet_types.h"
#include "mmap_file.h"

typedef enum LinkletWalOperation {
    LINKLET_WAL_INSERT_NODE = 0,
    LINKLET_WAL_INSERT_EDGE,
    LINKLET_WAL_UPDATE_NODE,
    LINKLET_WAL_UPDATE_EDGE,
    LINKLET_WAL_DELETE_NODE,
    LINKLET_WAL_DELETE_EDGE,
} LinkletWalOperation;

typedef struct LinkletResolvedOperation {
    LinkletWalOperation operation;
    uint64_t id;
    uint64_t source_id;
    uint64_t destination_id;
    bool detach;
    const LinkletBson *payload;
} LinkletResolvedOperation;

LinkletMmapFile *linklet_wal_open(const char *path, bool create, LinkletError *error);
void linklet_wal_close(LinkletMmapFile *wal);

bool linklet_wal_has_frame(const LinkletMmapFile *wal);

bool linklet_wal_write_frame(LinkletMmapFile *wal, uint64_t transaction_id,
                             const LinkletResolvedOperation *operations, size_t operation_count,
                             LinkletError *error);

bool linklet_wal_apply(LinkletGraphStore *store, const LinkletResolvedOperation *operations,
                       size_t operation_count, LinkletError *error);

bool linklet_wal_clear(LinkletMmapFile *wal, LinkletError *error);

bool linklet_wal_replay(LinkletMmapFile *wal, LinkletGraphStore *store, LinkletError *error);

#endif
