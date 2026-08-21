#ifndef LINKLET_TYPES_H
#define LINKLET_TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t LinkletId;

enum {
    LINKLET_ERROR_MESSAGE_CAPACITY = 256,
};

typedef struct LinkletError {
    char message[LINKLET_ERROR_MESSAGE_CAPACITY];
} LinkletError;

typedef struct LinkletIdList {
    uint64_t *ids;
    size_t count;
    size_t capacity;
} LinkletIdList;

void linklet_id_list_init(LinkletIdList *list);
bool linklet_id_list_append(LinkletIdList *list, uint64_t id);
void linklet_id_list_destroy(LinkletIdList *list);

typedef enum LinkletElementKind {
    LINKLET_ELEMENT_NODE = 0,
    LINKLET_ELEMENT_EDGE = 1,
} LinkletElementKind;

#endif
