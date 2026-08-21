#include "paged_object_file.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    LINKLET_OBJECT_FILE_VERSION = 2,
    LINKLET_OBJECT_LOCATION_INITIAL_CAPACITY = 16,
    LINKLET_OBJECT_CAPACITY_GROWTH_FACTOR = 2,
    LINKLET_OBJECT_PAGE_MAGIC = 0x4c4c5047u,
    LINKLET_OBJECT_PAGE_HEADER_SIZE = 32,
    LINKLET_OBJECT_SLOT_SIZE = 24,
    LINKLET_OBJECT_SLOT_ACTIVE = 1u,
    LINKLET_OBJECT_SLOT_DELETED = 2u,
};

typedef struct LinkletObjectPageHeader {
    uint32_t magic;
    uint32_t page_index;
    uint32_t slot_count;
    uint32_t free_start;
    uint32_t free_end;
    unsigned char reserved[LINKLET_OBJECT_PAGE_HEADER_SIZE - 5 * sizeof(uint32_t)];
} LinkletObjectPageHeader;

typedef struct LinkletObjectSlot {
    uint64_t id;
    uint32_t offset;
    uint32_t length;
    uint32_t capacity;
    uint32_t flags;
} LinkletObjectSlot;

_Static_assert(sizeof(LinkletObjectFileHeader) == LINKLET_OBJECT_FILE_HEADER_SIZE,
               "invalid object file header size");
_Static_assert(sizeof(LinkletObjectPageHeader) == LINKLET_OBJECT_PAGE_HEADER_SIZE,
               "invalid object page header size");
_Static_assert(sizeof(LinkletObjectSlot) == LINKLET_OBJECT_SLOT_SIZE, "invalid object slot size");

static void set_error(LinkletError *error, const char *format, ...) {
    if (!error) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
}

static bool ensure_location_capacity(LinkletPagedObjectFile *file, const size_t required) {
    if (required <= file->locations_capacity) {
        return true;
    }
    size_t capacity = file->locations_capacity ? file->locations_capacity
                                               : LINKLET_OBJECT_LOCATION_INITIAL_CAPACITY;
    while (capacity < required) {
        if (capacity > SIZE_MAX / LINKLET_OBJECT_CAPACITY_GROWTH_FACTOR) {
            return false;
        }
        capacity *= LINKLET_OBJECT_CAPACITY_GROWTH_FACTOR;
    }
    LinkletObjectLocation *locations =
        (LinkletObjectLocation *)realloc(file->locations, capacity * sizeof(*locations));
    if (!locations) {
        return false;
    }
    memset(locations + file->locations_capacity, 0,
           (capacity - file->locations_capacity) * sizeof(*locations));
    file->locations = locations;
    file->locations_capacity = capacity;
    return true;
}

static bool page_offset(const uint64_t page_index, uint64_t *offset) {
    if (page_index > (UINT64_MAX - LINKLET_OBJECT_FILE_HEADER_SIZE) / LINKLET_OBJECT_PAGE_SIZE) {
        return false;
    }
    *offset = LINKLET_OBJECT_FILE_HEADER_SIZE + page_index * LINKLET_OBJECT_PAGE_SIZE;
    return true;
}

static bool read_page_header(const LinkletPagedObjectFile *file, const uint32_t page_index,
                             LinkletObjectPageHeader *header, LinkletError *error) {
    uint64_t offset;
    return page_offset(page_index, &offset) &&
           linklet_mmap_read_at(file->file, header, sizeof(*header), offset, error);
}

static bool read_slot(const LinkletPagedObjectFile *file, const LinkletObjectLocation location,
                      LinkletObjectSlot *slot, LinkletError *error) {
    uint64_t page;
    if (!page_offset(location.page_index, &page)) {
        return false;
    }
    const uint64_t offset = page + sizeof(LinkletObjectPageHeader) +
                            (uint64_t)location.slot_index * sizeof(LinkletObjectSlot);
    return linklet_mmap_read_at(file->file, slot, sizeof(*slot), offset, error);
}

static bool write_file_header(LinkletPagedObjectFile *file, LinkletError *error) {
    return linklet_mmap_write_at(file->file, &file->header, sizeof(file->header), 0, error);
}

static bool validate_page_header(const LinkletObjectPageHeader *header,
                                 const uint32_t expected_page, LinkletError *error) {
    const uint64_t expected_free_start =
        sizeof(*header) + (uint64_t)header->slot_count * sizeof(LinkletObjectSlot);
    if (header->magic != LINKLET_OBJECT_PAGE_MAGIC || header->page_index != expected_page ||
        expected_free_start != header->free_start || header->free_start > header->free_end ||
        header->free_end > LINKLET_OBJECT_PAGE_SIZE) {
        set_error(error, "object page %u has an invalid header", expected_page);
        return false;
    }
    return true;
}

static bool append_page(LinkletPagedObjectFile *file, uint32_t *page_index, LinkletError *error) {
    if (file->header.page_count > UINT32_MAX) {
        set_error(error, "object file has too many pages");
        return false;
    }
    uint64_t offset;
    if (!page_offset(file->header.page_count, &offset) ||
        !linklet_mmap_resize(file->file, offset + LINKLET_OBJECT_PAGE_SIZE, error)) {
        return false;
    }
    LinkletObjectPageHeader page = {
        .magic = LINKLET_OBJECT_PAGE_MAGIC,
        .page_index = (uint32_t)file->header.page_count,
        .slot_count = 0,
        .free_start = sizeof(LinkletObjectPageHeader),
        .free_end = LINKLET_OBJECT_PAGE_SIZE,
    };
    if (!linklet_mmap_write_at(file->file, &page, sizeof(page), offset, error)) {
        return false;
    }
    *page_index = page.page_index;
    file->header.page_count++;
    return write_file_header(file, error);
}

static bool find_page_for_object(LinkletPagedObjectFile *file, const size_t bson_size,
                                 uint32_t *page_index, LinkletObjectPageHeader *page,
                                 LinkletError *error) {
    const size_t required = sizeof(LinkletObjectSlot) + bson_size;
    if (required > LINKLET_OBJECT_PAGE_SIZE - sizeof(LinkletObjectPageHeader)) {
        set_error(error, "BSON object is too large for a %u-byte object page",
                  LINKLET_OBJECT_PAGE_SIZE);
        return false;
    }
    for (uint64_t index = 0; index < file->header.page_count; ++index) {
        if (!read_page_header(file, (uint32_t)index, page, error) ||
            !validate_page_header(page, (uint32_t)index, error)) {
            if (!error || !error->message[0]) {
                set_error(error, "read object page %llu", (unsigned long long)index);
            }
            return false;
        }
        if ((size_t)(page->free_end - page->free_start) >= required) {
            *page_index = (uint32_t)index;
            return true;
        }
    }
    return append_page(file, page_index, error) && read_page_header(file, *page_index, page, error);
}

static bool place_object(LinkletPagedObjectFile *file, const uint64_t id, const LinkletBson *bson,
                         LinkletObjectLocation *location, LinkletError *error) {
    const size_t bson_size = linklet_bson_get_length(bson);
    uint32_t page_index;
    LinkletObjectPageHeader page;
    if (!find_page_for_object(file, bson_size, &page_index, &page, error)) {
        return false;
    }

    const uint32_t payload_offset = page.free_end - (uint32_t)bson_size;
    const uint32_t slot_index = page.slot_count;
    const LinkletObjectSlot slot = {
        .id = id,
        .offset = payload_offset,
        .length = (uint32_t)bson_size,
        .capacity = (uint32_t)bson_size,
        .flags = LINKLET_OBJECT_SLOT_ACTIVE,
    };
    uint64_t base;
    page_offset(page_index, &base);
    const uint64_t slot_offset = base + sizeof(page) + (uint64_t)slot_index * sizeof(slot);
    if (!linklet_mmap_write_at(file->file, linklet_bson_get_data(bson), bson_size,
                               base + payload_offset, error) ||
        !linklet_mmap_write_at(file->file, &slot, sizeof(slot), slot_offset, error)) {
        return false;
    }
    page.slot_count++;
    page.free_start += sizeof(LinkletObjectSlot);
    page.free_end = payload_offset;
    if (!linklet_mmap_write_at(file->file, &page, sizeof(page), base, error)) {
        return false;
    }
    *location = (LinkletObjectLocation){
        .page_index = page_index,
        .slot_index = slot_index,
        .alive = true,
    };
    return true;
}

bool linklet_object_file_initialize(LinkletPagedObjectFile *file, LinkletMmapFile *mmap,
                                    const char magic[LINKLET_STORAGE_MAGIC_SIZE],
                                    LinkletError *error) {
    *file = (LinkletPagedObjectFile){0};
    file->file = mmap;
    memcpy(file->header.magic, magic, LINKLET_STORAGE_MAGIC_SIZE);
    file->header.version = LINKLET_OBJECT_FILE_VERSION;
    file->header.page_size = LINKLET_OBJECT_PAGE_SIZE;
    if (!linklet_mmap_resize(mmap, LINKLET_OBJECT_FILE_HEADER_SIZE, error) ||
        !write_file_header(file, error)) {
        return false;
    }
    return true;
}

bool linklet_object_file_open(LinkletPagedObjectFile *file, LinkletMmapFile *mmap,
                              const char magic[LINKLET_STORAGE_MAGIC_SIZE], LinkletError *error) {
    *file = (LinkletPagedObjectFile){0};
    file->file = mmap;
    if (!linklet_mmap_read_at(mmap, &file->header, sizeof(file->header), 0, error) ||
        memcmp(file->header.magic, magic, LINKLET_STORAGE_MAGIC_SIZE) != 0 ||
        file->header.version != LINKLET_OBJECT_FILE_VERSION ||
        file->header.page_size != LINKLET_OBJECT_PAGE_SIZE || file->header.next_id > SIZE_MAX ||
        file->header.page_count > UINT32_MAX) {
        set_error(error, "object file has an incompatible header");
        return false;
    }
    uint64_t expected_size;
    if (!page_offset(file->header.page_count, &expected_size) ||
        linklet_mmap_size(mmap) != expected_size ||
        !ensure_location_capacity(file, (size_t)file->header.next_id)) {
        set_error(error, "object file size or ID index is invalid");
        return false;
    }

    uint64_t live = 0;
    for (uint32_t page_index = 0; page_index < (uint32_t)file->header.page_count; ++page_index) {
        LinkletObjectPageHeader page;
        if (!read_page_header(file, page_index, &page, error) ||
            !validate_page_header(&page, page_index, error)) {
            return false;
        }
        uint64_t base;
        page_offset(page_index, &base);
        for (uint32_t slot_index = 0; slot_index < page.slot_count; ++slot_index) {
            LinkletObjectSlot slot;
            const uint64_t slot_offset = base + sizeof(page) + (uint64_t)slot_index * sizeof(slot);
            if (!linklet_mmap_read_at(mmap, &slot, sizeof(slot), slot_offset, error) ||
                slot.id >= file->header.next_id || slot.length > slot.capacity ||
                slot.offset < page.free_end ||
                (uint64_t)slot.offset + slot.capacity > LINKLET_OBJECT_PAGE_SIZE) {
                set_error(error, "object page %u slot %u is invalid", page_index, slot_index);
                return false;
            }
            if (slot.flags != LINKLET_OBJECT_SLOT_ACTIVE) {
                continue;
            }
            if (file->locations[slot.id].alive) {
                set_error(error, "object ID %llu has multiple active slots",
                          (unsigned long long)slot.id);
                return false;
            }
            uint8_t *bytes = (uint8_t *)malloc(slot.length);
            LinkletBsonError bson_error;
            if (!bytes ||
                !linklet_mmap_read_at(mmap, bytes, slot.length, base + slot.offset, error) ||
                !linklet_bson_validate(bytes, slot.length, &bson_error)) {
                free(bytes);
                set_error(error, "object ID %llu contains invalid BSON",
                          (unsigned long long)slot.id);
                return false;
            }
            free(bytes);
            file->locations[slot.id] = (LinkletObjectLocation){
                .page_index = page_index,
                .slot_index = slot_index,
                .alive = true,
            };
            live++;
        }
    }
    if (live != file->header.live_count) {
        set_error(error, "object file live-object count is inconsistent");
        return false;
    }
    return true;
}

void linklet_object_file_destroy(LinkletPagedObjectFile *file) {
    if (!file) {
        return;
    }
    free(file->locations);
    *file = (LinkletPagedObjectFile){0};
}

bool linklet_object_file_insert(LinkletPagedObjectFile *file, const LinkletBson *bson, uint64_t *id,
                                LinkletError *error) {
    if (!file || !bson || !id) {
        set_error(error, "object file, BSON, and ID output are required");
        return false;
    }
    LinkletBsonError bson_error;
    if (!linklet_bson_validate_document(bson, &bson_error)) {
        set_error(error, "cannot store invalid BSON: %s", bson_error.message);
        return false;
    }
    if (file->header.next_id >= SIZE_MAX ||
        !ensure_location_capacity(file, (size_t)file->header.next_id + 1)) {
        set_error(error, "object ID index is full");
        return false;
    }
    const uint64_t new_id = file->header.next_id;
    LinkletObjectLocation location;
    if (!place_object(file, new_id, bson, &location, error)) {
        return false;
    }
    file->locations[new_id] = location;
    file->header.next_id++;
    file->header.live_count++;
    if (!write_file_header(file, error)) {
        return false;
    }
    *id = new_id;
    return true;
}

bool linklet_object_file_read(const LinkletPagedObjectFile *file, const uint64_t id,
                              LinkletBson *bson, LinkletError *error) {
    if (!file || !bson || id >= file->header.next_id || !file->locations[id].alive) {
        set_error(error, "object ID does not exist");
        return false;
    }
    LinkletObjectSlot slot;
    const LinkletObjectLocation location = file->locations[id];
    uint64_t base;
    if (!read_slot(file, location, &slot, error) || slot.id != id ||
        slot.flags != LINKLET_OBJECT_SLOT_ACTIVE || !page_offset(location.page_index, &base)) {
        set_error(error, "object slot is invalid");
        return false;
    }
    uint8_t *bytes = (uint8_t *)malloc(slot.length);
    if (!bytes ||
        !linklet_mmap_read_at(file->file, bytes, slot.length, base + slot.offset, error)) {
        free(bytes);
        return false;
    }
    linklet_bson_init(bson);
    bson->data = bytes;
    bson->length = slot.length;
    bson->capacity = slot.length;
    return true;
}

static bool mark_location_deleted(LinkletPagedObjectFile *file,
                                  const LinkletObjectLocation location, LinkletError *error) {
    LinkletObjectSlot slot;
    uint64_t base;
    if (!read_slot(file, location, &slot, error) || !page_offset(location.page_index, &base)) {
        return false;
    }
    slot.flags = LINKLET_OBJECT_SLOT_DELETED;
    const uint64_t offset =
        base + sizeof(LinkletObjectPageHeader) + (uint64_t)location.slot_index * sizeof(slot);
    return linklet_mmap_write_at(file->file, &slot, sizeof(slot), offset, error);
}

bool linklet_object_file_update(LinkletPagedObjectFile *file, const uint64_t id,
                                const LinkletBson *bson, LinkletError *error) {
    if (!file || !bson || id >= file->header.next_id || !file->locations[id].alive) {
        set_error(error, "object ID or replacement BSON is invalid");
        return false;
    }
    LinkletBsonError bson_error;
    if (!linklet_bson_validate_document(bson, &bson_error)) {
        set_error(error, "replacement BSON is invalid: %s", bson_error.message);
        return false;
    }
    const LinkletObjectLocation old_location = file->locations[id];
    LinkletObjectSlot old_slot;
    uint64_t base;
    if (!read_slot(file, old_location, &old_slot, error) ||
        !page_offset(old_location.page_index, &base)) {
        return false;
    }
    const size_t bson_size = linklet_bson_get_length(bson);
    if (bson_size <= old_slot.capacity) {
        old_slot.length = (uint32_t)bson_size;
        const uint64_t slot_offset = base + sizeof(LinkletObjectPageHeader) +
                                     (uint64_t)old_location.slot_index * sizeof(old_slot);
        if (!linklet_mmap_write_at(file->file, linklet_bson_get_data(bson), bson_size,
                                   base + old_slot.offset, error) ||
            !linklet_mmap_write_at(file->file, &old_slot, sizeof(old_slot), slot_offset, error)) {
            return false;
        }
        return true;
    }

    LinkletObjectLocation new_location;
    if (!place_object(file, id, bson, &new_location, error) ||
        !mark_location_deleted(file, old_location, error)) {
        return false;
    }
    file->locations[id] = new_location;
    return true;
}

bool linklet_object_file_delete(LinkletPagedObjectFile *file, const uint64_t id,
                                LinkletError *error) {
    if (!file || id >= file->header.next_id || !file->locations[id].alive) {
        set_error(error, "object ID does not exist");
        return false;
    }
    if (!mark_location_deleted(file, file->locations[id], error)) {
        return false;
    }
    file->locations[id].alive = false;
    file->header.live_count--;
    return write_file_header(file, error);
}

bool linklet_object_file_exists(const LinkletPagedObjectFile *file, const uint64_t id) {
    return file && id < file->header.next_id && file->locations[id].alive;
}
