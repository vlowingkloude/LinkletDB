#ifndef LINKLET_MMAP_FILE_H
#define LINKLET_MMAP_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "linklet_types.h"

typedef struct LinkletMmapFile LinkletMmapFile;

typedef struct LinkletMmapView {
    void *addr;
    size_t size;
} LinkletMmapView;

LinkletMmapFile *linklet_mmap_create(const char *path, LinkletError *error);

LinkletMmapFile *linklet_mmap_open(const char *path, bool writable, LinkletError *error);

void linklet_mmap_close(LinkletMmapFile *file);

uint64_t linklet_mmap_size(const LinkletMmapFile *file);

bool linklet_mmap_resize(LinkletMmapFile *file, uint64_t new_size, LinkletError *error);

bool linklet_mmap_read_at(const LinkletMmapFile *file, void *destination, size_t size,
                          uint64_t offset, LinkletError *error);
bool linklet_mmap_write_at(LinkletMmapFile *file, const void *source, size_t size, uint64_t offset,
                           LinkletError *error);

bool linklet_mmap_map(LinkletMmapFile *file, uint64_t offset, size_t length, bool writable,
                      LinkletMmapView *view, LinkletError *error);
bool linklet_mmap_unmap(LinkletMmapView *view, LinkletError *error);

bool linklet_mmap_sync(LinkletMmapView *view, LinkletError *error);

bool linklet_mmap_flush(LinkletMmapFile *file, LinkletError *error);

#endif
