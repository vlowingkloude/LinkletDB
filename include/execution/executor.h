#ifndef LINKLET_EXECUTOR_H
#define LINKLET_EXECUTOR_H

#include <stdbool.h>
#include <stdint.h>

#include "graph_store.h"
#include "kernel.h"

typedef enum LinkletResultKind {
    LINKLET_RESULT_IDS = 0,
    LINKLET_RESULT_BOOL,
    LINKLET_RESULT_COUNT,
} LinkletResultKind;

typedef struct LinkletResult {
    LinkletResultKind kind;
    LinkletElementKind element_kind;
    LinkletIdList ids;
    bool boolean;
    uint64_t inserted_id;
    size_t affected_count;
} LinkletResult;

bool linklet_execute(const LinkletLogicalPlan *plan, LinkletGraphStore *store,
                     LinkletResult *result, LinkletError *error);

bool linklet_resolve_match(const LinkletGraphStore *store, const LinkletMatchPattern *pattern,
                           LinkletDirection direction, LinkletIdList *ids, LinkletError *error);

void linklet_result_destroy(LinkletResult *result);

#endif
