#ifndef LINKLET_OPERATORS_H
#define LINKLET_OPERATORS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "physical_coo.h"

bool operator_is_reachable_flat_coo_bounded(const LinkletFlatCoo *coo, uint64_t source_id,
                                            uint64_t destination_id, size_t max_hops, bool reverse,
                                            unsigned char *frontier, unsigned char *next_frontier,
                                            size_t vertex_count);

#endif
