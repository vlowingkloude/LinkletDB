#ifndef LINKLETDB_PHYSICAL_COO_H
#define LINKLETDB_PHYSICAL_COO_H

#include <stdint.h>
#include <stdbool.h>

typedef struct LinkletCooEdge {
    uint64_t source_id;
    uint64_t destination_id;
} LinkletCooEdge;

typedef struct LinkletFlatCoo {
    size_t edge_count;
    LinkletCooEdge *edges;
    size_t vertex_id_capacity;
} LinkletFlatCoo;

static inline bool linklet_coo_edge_is_deleted(const LinkletCooEdge edge) {
    return edge.source_id == UINT64_MAX && edge.destination_id == UINT64_MAX;
}

#endif
