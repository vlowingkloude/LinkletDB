#include <stdio.h>
#include <string.h>

#include "advisor.h"

bool linklet_advise(const LinkletKernelCall *call, const LinkletFlatCoo *coo,
                    LinkletExecutionPlan *plan, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!call || !coo || !plan) {
        if (error) {
            snprintf(error->message, sizeof(error->message),
                     "kernel call, COO storage, and execution plan are required");
        }
        return false;
    }
    *plan = (LinkletExecutionPlan){0};

    switch (call->code) {
    case LINKLET_KERNEL_REACHABILITY: {
        if (coo->vertex_id_capacity == 0 || call->source_id >= coo->vertex_id_capacity ||
            call->destination_id >= coo->vertex_id_capacity) {
            if (error) {
                snprintf(error->message, sizeof(error->message),
                         "reachability endpoint lies outside the COO vertex ID domain");
            }
            return false;
        }
        if (coo->vertex_id_capacity > SIZE_MAX / LINKLET_REACHABILITY_FRONTIER_COUNT) {
            if (error) {
                snprintf(error->message, sizeof(error->message),
                         "reachability frontier size overflows size_t");
            }
            return false;
        }
        plan->scratch_capacity = coo->vertex_id_capacity;
        plan->result_capacity = 1;
        plan->parallel = false;
        return true;
    }
    case LINKLET_KERNEL_MATCH:
    case LINKLET_KERNEL_INSERT:
    case LINKLET_KERNEL_UPDATE:
    case LINKLET_KERNEL_DELETE:

        return true;
    default:
        if (error) {
            snprintf(error->message, sizeof(error->message), "unknown kernel code %d",
                     (int)call->code);
        }
        return false;
    }
}
