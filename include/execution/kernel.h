#ifndef LINKLET_KERNEL_H
#define LINKLET_KERNEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "bson.h"
#include "linklet_types.h"

typedef enum LinkletDirection {
    LINKLET_DIR_RIGHT = 0,
    LINKLET_DIR_LEFT = 1,
} LinkletDirection;

typedef enum LinkletKernelCode {
    LINKLET_KERNEL_MATCH = 0,
    LINKLET_KERNEL_REACHABILITY,
    LINKLET_KERNEL_INSERT,
    LINKLET_KERNEL_UPDATE,
    LINKLET_KERNEL_DELETE,
} LinkletKernelCode;

typedef struct LinkletObjectFilter {
    bool has_id;
    uint64_t id;
    char *label;
    LinkletBson properties;
} LinkletObjectFilter;

typedef enum LinkletMatchBinding {
    LINKLET_MATCH_SOURCE = 0,
    LINKLET_MATCH_EDGE,
    LINKLET_MATCH_DESTINATION,
} LinkletMatchBinding;

typedef struct LinkletMatchPattern {
    bool is_edge_pattern;
    LinkletObjectFilter source;
    LinkletObjectFilter edge;
    LinkletObjectFilter destination;
    LinkletElementKind result_kind;
    LinkletMatchBinding result_binding;
} LinkletMatchPattern;

typedef struct LinkletKernelCall {
    LinkletKernelCode code;

    LinkletDirection direction;

    LinkletMatchPattern match;

    uint64_t source_id;
    uint64_t destination_id;
    size_t max_hops;

    LinkletElementKind insert_kind;
    uint64_t insert_source_id;
    uint64_t insert_destination_id;
    LinkletBson payload;

    bool detach;
} LinkletKernelCall;

typedef struct LinkletLogicalPlan {
    LinkletKernelCall *calls;
    size_t call_count;
} LinkletLogicalPlan;

void linklet_kernel_call_destroy(LinkletKernelCall *call);
void linklet_logical_plan_destroy(LinkletLogicalPlan *plan);

void linklet_logical_plan_dump(const LinkletLogicalPlan *plan, FILE *out);

#endif
