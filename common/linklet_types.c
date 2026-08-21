#include <stdlib.h>
#include <string.h>

#include "linklet_types.h"

enum {
    LINKLET_ID_LIST_INITIAL_CAPACITY = 16,
    LINKLET_CAPACITY_GROWTH_FACTOR = 2,
};

void linklet_id_list_init(LinkletIdList *list) {
    if (list) {
        *list = (LinkletIdList){0};
    }
}

bool linklet_id_list_append(LinkletIdList *list, const uint64_t id) {
    if (!list) {
        return false;
    }
    if (list->count == list->capacity) {
        const size_t capacity = list->capacity ? list->capacity * LINKLET_CAPACITY_GROWTH_FACTOR
                                               : LINKLET_ID_LIST_INITIAL_CAPACITY;
        if (capacity < list->capacity) {
            return false;
        }
        uint64_t *ids = (uint64_t *)realloc(list->ids, capacity * sizeof(*ids));
        if (!ids) {
            return false;
        }
        list->ids = ids;
        list->capacity = capacity;
    }
    list->ids[list->count++] = id;
    return true;
}

void linklet_id_list_destroy(LinkletIdList *list) {
    if (!list) {
        return;
    }
    free(list->ids);
    *list = (LinkletIdList){0};
}
