#ifndef LINKLET_PAGED_OBJECT_FILE_H
#define LINKLET_PAGED_OBJECT_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bson.h"
#include "linklet_types.h"
#include "mmap_file.h"

enum {
    LINKLET_STORAGE_MAGIC_SIZE = 8,
    LINKLET_OBJECT_FILE_HEADER_SIZE = 64,
    LINKLET_OBJECT_PAGE_SIZE = 64 * 1024,
};

typedef struct LinkletObjectFileHeader {
    char magic[LINKLET_STORAGE_MAGIC_SIZE];
    uint32_t version;
    uint32_t page_size;
    uint64_t page_count;
    uint64_t next_id;
    uint64_t live_count;
    unsigned char reserved[LINKLET_OBJECT_FILE_HEADER_SIZE - LINKLET_STORAGE_MAGIC_SIZE -
                           2 * sizeof(uint32_t) - 3 * sizeof(uint64_t)];
} LinkletObjectFileHeader;

typedef struct LinkletObjectLocation {
    uint32_t page_index;
    uint32_t slot_index;
    bool alive;
} LinkletObjectLocation;

typedef struct LinkletPagedObjectFile {
    LinkletMmapFile *file;
    LinkletObjectFileHeader header;
    LinkletObjectLocation *locations;
    size_t locations_capacity;
} LinkletPagedObjectFile;

bool linklet_object_file_initialize(LinkletPagedObjectFile *file, LinkletMmapFile *mmap,
                                    const char magic[LINKLET_STORAGE_MAGIC_SIZE],
                                    LinkletError *error);
bool linklet_object_file_open(LinkletPagedObjectFile *file, LinkletMmapFile *mmap,
                              const char magic[LINKLET_STORAGE_MAGIC_SIZE], LinkletError *error);
void linklet_object_file_destroy(LinkletPagedObjectFile *file);

bool linklet_object_file_insert(LinkletPagedObjectFile *file, const LinkletBson *bson, uint64_t *id,
                                LinkletError *error);
bool linklet_object_file_read(const LinkletPagedObjectFile *file, uint64_t id, LinkletBson *bson,
                              LinkletError *error);
bool linklet_object_file_update(LinkletPagedObjectFile *file, uint64_t id, const LinkletBson *bson,
                                LinkletError *error);
bool linklet_object_file_delete(LinkletPagedObjectFile *file, uint64_t id, LinkletError *error);
bool linklet_object_file_exists(const LinkletPagedObjectFile *file, uint64_t id);

#endif
