#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "advisor.h"
#include "kernel.h"
#include "naive_tests.h"
#include "operators.h"
#include "physical_coo.h"

bool test_operator_reachability_bounded(void) {
    LinkletCooEdge edges[] = {
        {.source_id = 1, .destination_id = 2},
        {.source_id = 8, .destination_id = 9},
        {.source_id = 2, .destination_id = 3},
        {.source_id = 3, .destination_id = 4},
    };
    LinkletFlatCoo coo = {.edge_count = 4, .edges = edges, .vertex_id_capacity = 10};
    unsigned char frontier[10];
    unsigned char next_frontier[10];

    bool ok =
        operator_is_reachable_flat_coo_bounded(&coo, 1, 4, 3, false, frontier, next_frontier, 10);
    ok = ok &&
         !operator_is_reachable_flat_coo_bounded(&coo, 1, 4, 2, false, frontier, next_frontier, 10);
    ok = ok &&
         operator_is_reachable_flat_coo_bounded(&coo, 4, 1, 3, true, frontier, next_frontier, 10);

    ok = ok &&
         !operator_is_reachable_flat_coo_bounded(&coo, 1, 1, 3, false, frontier, next_frontier, 10);
    ok = ok &&
         !operator_is_reachable_flat_coo_bounded(&coo, 9, 1, 3, false, frontier, next_frontier, 10);

    LinkletCooEdge with_tombstone[] = {
        {.source_id = 1, .destination_id = 2},
        {.source_id = UINT64_MAX, .destination_id = UINT64_MAX},
        {.source_id = 2, .destination_id = 3},
    };
    LinkletFlatCoo tomb = {.edge_count = 3, .edges = with_tombstone, .vertex_id_capacity = 10};
    ok = ok &&
         operator_is_reachable_flat_coo_bounded(&tomb, 1, 3, 2, false, frontier, next_frontier, 10);
    return ok;
}

bool test_advisor_rejects_out_of_domain_endpoint(void) {
    LinkletCooEdge edge = {.source_id = 1, .destination_id = 2};
    LinkletFlatCoo coo = {.edge_count = 1, .edges = &edge, .vertex_id_capacity = 3};

    LinkletKernelCall call;
    call = (LinkletKernelCall){0};
    call.code = LINKLET_KERNEL_REACHABILITY;
    call.source_id = 1;
    call.destination_id = 99;
    call.max_hops = 2;

    LinkletExecutionPlan plan;
    LinkletError error;
    return !linklet_advise(&call, &coo, &plan, &error);
}
