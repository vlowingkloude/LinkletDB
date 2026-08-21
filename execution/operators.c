#include "operators.h"

#include <string.h>

bool operator_is_reachable_flat_coo_bounded(const LinkletFlatCoo *const coo,
                                            const uint64_t source_id, const uint64_t destination_id,
                                            const size_t max_hops, const bool reverse,
                                            unsigned char *frontier, unsigned char *next_frontier,
                                            const size_t vertex_count) {
    if (!coo || !frontier || !next_frontier || max_hops == 0 || source_id >= vertex_count ||
        destination_id >= vertex_count) {
        return false;
    }

    memset(frontier, 0, vertex_count);
    memset(next_frontier, 0, vertex_count);
    frontier[source_id] = 1;

    for (size_t hop = 0; hop < max_hops; ++hop) {
        memset(next_frontier, 0, vertex_count);
        bool has_next_frontier = false;
        for (size_t edge_index = 0; edge_index < coo->edge_count; ++edge_index) {
            const LinkletCooEdge edge = coo->edges[edge_index];
            if (linklet_coo_edge_is_deleted(edge)) {
                continue;
            }
            const uint64_t from = reverse ? edge.destination_id : edge.source_id;
            const uint64_t to = reverse ? edge.source_id : edge.destination_id;
            if (frontier[from] != 0) {
                next_frontier[to] = 1;
                has_next_frontier = true;
            }
        }
        if (next_frontier[destination_id] != 0) {
            return true;
        }
        if (!has_next_frontier) {
            return false;
        }

        unsigned char *swap = frontier;
        frontier = next_frontier;
        next_frontier = swap;
    }
    return false;
}
