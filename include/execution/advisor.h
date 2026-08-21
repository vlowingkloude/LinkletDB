#ifndef LINKLET_ADVISOR_H
#define LINKLET_ADVISOR_H

#include <stdbool.h>

#include "kernel.h"
#include "physical_coo.h"

enum {
    LINKLET_REACHABILITY_FRONTIER_COUNT = 2,
};

typedef struct LinkletExecutionPlan {
    size_t scratch_capacity;
    size_t result_capacity;
    bool parallel;
} LinkletExecutionPlan;

bool linklet_advise(const LinkletKernelCall *call, const LinkletFlatCoo *coo,
                    LinkletExecutionPlan *plan, LinkletError *error);

#endif
