#include "graph_store.h"

#include "paged_object_file.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum {
    LINKLET_FILE_VERSION = 2,
    LINKLET_HEADER_SIZE = 64,
    LINKLET_DEFAULT_EDGE_CAPACITY = 16,
    LINKLET_EDGE_CAPACITY_GROWTH_FACTOR = 2,
};

typedef struct LinkletConnectivityHeader {
    char magic[LINKLET_STORAGE_MAGIC_SIZE];
    uint32_t version;
    uint32_t edge_size;
    uint64_t edge_count;
    uint64_t edge_capacity;
    uint64_t vertex_id_capacity;
    unsigned char reserved[LINKLET_HEADER_SIZE - LINKLET_STORAGE_MAGIC_SIZE - 2 * sizeof(uint32_t) -
                           3 * sizeof(uint64_t)];
} LinkletConnectivityHeader;

_Static_assert(sizeof(LinkletCooEdge) == 2 * sizeof(uint64_t),
               "flat COO records must remain 16 bytes");
_Static_assert(sizeof(LinkletConnectivityHeader) == LINKLET_HEADER_SIZE,
               "invalid connectivity header size");

struct LinkletGraphStore {
    char *path;
    LinkletMmapFile *connectivity;
    LinkletMmapView connectivity_view;
    LinkletConnectivityHeader *connectivity_header;
    LinkletCooEdge *coo_edges;
    LinkletPagedObjectFile nodes;
    LinkletPagedObjectFile edges;
};

static const char CONNECTIVITY_MAGIC[LINKLET_STORAGE_MAGIC_SIZE] = {'L', 'N', 'K', 'C',
                                                                    'O', 'O', '2', '\0'};
static const char NODES_MAGIC[LINKLET_STORAGE_MAGIC_SIZE] = {'L', 'N', 'K', 'N',
                                                             'O', 'D', '2', '\0'};
static const char EDGES_MAGIC[LINKLET_STORAGE_MAGIC_SIZE] = {'L', 'N', 'K', 'E',
                                                             'D', 'G', '2', '\0'};

static void set_error(LinkletError *error, const char *format, ...) {
    if (!error) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
}

static char *store_file_path(const LinkletGraphStore *store, const char *name) {
    const size_t size = strlen(store->path) + 1 + strlen(name) + 1;
    char *buffer = (char *)malloc(size);
    if (buffer) {
        snprintf(buffer, size, "%s/%s", store->path, name);
    }
    return buffer;
}

static bool checked_connectivity_size(const size_t capacity, size_t *size) {
    if (capacity > (SIZE_MAX - LINKLET_HEADER_SIZE) / sizeof(LinkletCooEdge)) {
        return false;
    }
    *size = LINKLET_HEADER_SIZE + capacity * sizeof(LinkletCooEdge);
    return true;
}

static LinkletGraphStore *allocate_store(LinkletError *error) {
    LinkletGraphStore *store = (LinkletGraphStore *)calloc(1, sizeof(*store));
    if (!store) {
        set_error(error, "could not allocate graph storage");
    }
    return store;
}

static bool map_connectivity(LinkletGraphStore *store, LinkletError *error) {
    const uint64_t size = linklet_mmap_size(store->connectivity);
    if (size < LINKLET_HEADER_SIZE) {
        set_error(error, "connectivity.ll is shorter than its header");
        return false;
    }
    if (!linklet_mmap_map(store->connectivity, 0, (size_t)size, true, &store->connectivity_view,
                          error)) {
        return false;
    }
    store->connectivity_header = (LinkletConnectivityHeader *)store->connectivity_view.addr;
    store->coo_edges =
        (LinkletCooEdge *)((unsigned char *)store->connectivity_view.addr + LINKLET_HEADER_SIZE);
    return true;
}

static bool initialize_connectivity(LinkletGraphStore *store, size_t capacity,
                                    LinkletError *error) {
    if (capacity == 0) {
        capacity = LINKLET_DEFAULT_EDGE_CAPACITY;
    }
    size_t file_size;
    if (!checked_connectivity_size(capacity, &file_size)) {
        set_error(error, "initial edge capacity is too large");
        return false;
    }
    if (!linklet_mmap_resize(store->connectivity, file_size, error) ||
        !map_connectivity(store, error)) {
        return false;
    }
    memset(store->connectivity_view.addr, 0, file_size);
    memcpy(store->connectivity_header->magic, CONNECTIVITY_MAGIC, sizeof(CONNECTIVITY_MAGIC));
    store->connectivity_header->version = LINKLET_FILE_VERSION;
    store->connectivity_header->edge_size = sizeof(LinkletCooEdge);
    store->connectivity_header->edge_capacity = capacity;
    return true;
}

static bool grow_connectivity(LinkletGraphStore *store, LinkletError *error) {
    const uint64_t old_capacity = store->connectivity_header->edge_capacity;
    if (old_capacity > SIZE_MAX / LINKLET_EDGE_CAPACITY_GROWTH_FACTOR) {
        set_error(error, "connectivity capacity overflows size_t");
        return false;
    }
    const size_t new_capacity = old_capacity
                                    ? (size_t)old_capacity * LINKLET_EDGE_CAPACITY_GROWTH_FACTOR
                                    : LINKLET_DEFAULT_EDGE_CAPACITY;
    size_t new_size;
    if (!checked_connectivity_size(new_capacity, &new_size)) {
        set_error(error, "connectivity file is too large");
        return false;
    }
    if (!linklet_mmap_sync(&store->connectivity_view, error) ||
        !linklet_mmap_unmap(&store->connectivity_view, error)) {
        return false;
    }
    store->connectivity_header = NULL;
    store->coo_edges = NULL;
    if (!linklet_mmap_resize(store->connectivity, new_size, error) ||
        !map_connectivity(store, error)) {
        return false;
    }
    store->connectivity_header->edge_capacity = new_capacity;
    return true;
}

static bool validate_graph_files(const LinkletGraphStore *store, LinkletError *error) {
    if (store->connectivity_header->edge_count != store->edges.header.next_id ||
        store->connectivity_header->vertex_id_capacity != store->nodes.header.next_id) {
        set_error(error, "graph file ID domains are inconsistent");
        return false;
    }
    for (uint64_t edge_id = 0; edge_id < store->connectivity_header->edge_count; ++edge_id) {
        const bool object_exists = linklet_object_file_exists(&store->edges, edge_id);
        const LinkletCooEdge edge = store->coo_edges[edge_id];
        if (object_exists == linklet_coo_edge_is_deleted(edge)) {
            set_error(error, "edge %llu property and connectivity records disagree",
                      (unsigned long long)edge_id);
            return false;
        }
        if (object_exists && (!linklet_object_file_exists(&store->nodes, edge.source_id) ||
                              !linklet_object_file_exists(&store->nodes, edge.destination_id))) {
            set_error(error, "edge %llu references a deleted node", (unsigned long long)edge_id);
            return false;
        }
    }
    return true;
}

LinkletGraphStore *linklet_graph_store_create(const char *path, const size_t initial_edge_capacity,
                                              LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!path || !path[0]) {
        set_error(error, "graph path is required");
        return NULL;
    }
    if (mkdir(path, 0755) != 0) {
        set_error(error, "create graph directory: %s", strerror(errno));
        return NULL;
    }
    LinkletGraphStore *store = allocate_store(error);
    if (!store) {
        return NULL;
    }
    store->path = strdup(path);

    char *connectivity_path = store_file_path(store, "connectivity.ll");
    char *nodes_path = store_file_path(store, "nodes.ll");
    char *edges_path = store_file_path(store, "edges.ll");
    store->connectivity = connectivity_path ? linklet_mmap_create(connectivity_path, error) : NULL;
    LinkletMmapFile *nodes_file = nodes_path ? linklet_mmap_create(nodes_path, error) : NULL;
    LinkletMmapFile *edges_file = edges_path ? linklet_mmap_create(edges_path, error) : NULL;
    free(connectivity_path);
    free(nodes_path);
    free(edges_path);

    if (!store->connectivity || !nodes_file || !edges_file ||
        !initialize_connectivity(store, initial_edge_capacity, error) ||
        !linklet_object_file_initialize(&store->nodes, nodes_file, NODES_MAGIC, error) ||
        !linklet_object_file_initialize(&store->edges, edges_file, EDGES_MAGIC, error)) {
        if (!error || !error->message[0]) {
            set_error(error, "create graph files");
        }
        if (store->connectivity_view.addr) {
            linklet_mmap_unmap(&store->connectivity_view, NULL);
        }
        if (store->connectivity) {
            linklet_mmap_close(store->connectivity);
        }
        if (nodes_file) {
            linklet_mmap_close(nodes_file);
        }
        if (edges_file) {
            linklet_mmap_close(edges_file);
        }
        free(store->path);
        free(store);
        return NULL;
    }
    return store;
}

LinkletGraphStore *linklet_graph_store_open(const char *path, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    LinkletGraphStore *store = allocate_store(error);
    if (!store) {
        return NULL;
    }
    store->path = strdup(path);

    char *connectivity_path = store_file_path(store, "connectivity.ll");
    char *nodes_path = store_file_path(store, "nodes.ll");
    char *edges_path = store_file_path(store, "edges.ll");
    store->connectivity =
        connectivity_path ? linklet_mmap_open(connectivity_path, true, error) : NULL;
    LinkletMmapFile *nodes_file = nodes_path ? linklet_mmap_open(nodes_path, true, error) : NULL;
    LinkletMmapFile *edges_file = edges_path ? linklet_mmap_open(edges_path, true, error) : NULL;
    free(connectivity_path);
    free(nodes_path);
    free(edges_path);

    size_t expected_size;
    if (!store->connectivity || !nodes_file || !edges_file || !map_connectivity(store, error) ||
        memcmp(store->connectivity_header->magic, CONNECTIVITY_MAGIC, sizeof(CONNECTIVITY_MAGIC)) !=
            0 ||
        store->connectivity_header->version != LINKLET_FILE_VERSION ||
        store->connectivity_header->edge_size != sizeof(LinkletCooEdge) ||
        store->connectivity_header->edge_count > store->connectivity_header->edge_capacity ||
        store->connectivity_header->edge_capacity > SIZE_MAX ||
        !checked_connectivity_size((size_t)store->connectivity_header->edge_capacity,
                                   &expected_size) ||
        expected_size != linklet_mmap_size(store->connectivity) ||
        !linklet_object_file_open(&store->nodes, nodes_file, NODES_MAGIC, error) ||
        !linklet_object_file_open(&store->edges, edges_file, EDGES_MAGIC, error) ||
        !validate_graph_files(store, error)) {
        if (!error || !error->message[0]) {
            set_error(error, "graph files are inconsistent");
        }
        if (store->connectivity_view.addr) {
            linklet_mmap_unmap(&store->connectivity_view, NULL);
        }
        if (store->connectivity) {
            linklet_mmap_close(store->connectivity);
        }
        if (nodes_file) {
            linklet_mmap_close(nodes_file);
        }
        if (edges_file) {
            linklet_mmap_close(edges_file);
        }
        free(store->path);
        free(store);
        return NULL;
    }
    return store;
}

bool linklet_graph_store_close(LinkletGraphStore *store, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!store) {
        return true;
    }

    LinkletMmapFile *nodes_file = store->nodes.file;
    LinkletMmapFile *edges_file = store->edges.file;
    bool success = true;
    if (store->connectivity_view.addr && !linklet_mmap_sync(&store->connectivity_view, error)) {
        success = false;
    }
    if (store->connectivity && !linklet_mmap_flush(store->connectivity, error)) {
        if (success) {
            success = false;
        }
    }
    if (nodes_file && !linklet_mmap_flush(nodes_file, error)) {
        if (success) {
            success = false;
        }
    }
    if (edges_file && !linklet_mmap_flush(edges_file, error)) {
        if (success) {
            success = false;
        }
    }

    if (store->connectivity_view.addr) {
        linklet_mmap_unmap(&store->connectivity_view, NULL);
    }
    linklet_object_file_destroy(&store->nodes);
    linklet_object_file_destroy(&store->edges);
    if (store->connectivity) {
        linklet_mmap_close(store->connectivity);
    }
    if (nodes_file) {
        linklet_mmap_close(nodes_file);
    }
    if (edges_file) {
        linklet_mmap_close(edges_file);
    }
    free(store->path);
    free(store);
    return success;
}

bool linklet_graph_store_insert_node(LinkletGraphStore *store, const LinkletBson *bson,
                                     LinkletNodeId *node_id, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!store || !node_id) {
        set_error(error, "graph store and node ID output are required");
        return false;
    }
    if (!linklet_object_file_insert(&store->nodes, bson, node_id, error)) {
        return false;
    }
    store->connectivity_header->vertex_id_capacity = store->nodes.header.next_id;
    return true;
}

bool linklet_graph_store_insert_edge(LinkletGraphStore *store, const LinkletNodeId source_id,
                                     const LinkletNodeId destination_id, const LinkletBson *bson,
                                     LinkletEdgeId *edge_id, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!store || !edge_id || !linklet_object_file_exists(&store->nodes, source_id) ||
        !linklet_object_file_exists(&store->nodes, destination_id)) {
        set_error(error, "edge endpoint does not exist");
        return false;
    }
    if (store->connectivity_header->edge_count == store->connectivity_header->edge_capacity &&
        !grow_connectivity(store, error)) {
        return false;
    }
    uint64_t id;
    if (!linklet_object_file_insert(&store->edges, bson, &id, error)) {
        return false;
    }
    if (id != store->connectivity_header->edge_count) {
        set_error(error, "edge property and connectivity ID domains diverged");
        return false;
    }
    store->coo_edges[id] =
        (LinkletCooEdge){.source_id = source_id, .destination_id = destination_id};
    store->connectivity_header->edge_count = id + 1;
    *edge_id = id;
    return true;
}

bool linklet_graph_store_update_node(LinkletGraphStore *store, const LinkletNodeId node_id,
                                     const LinkletBson *bson, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    return store && linklet_object_file_update(&store->nodes, node_id, bson, error);
}

bool linklet_graph_store_update_edge(LinkletGraphStore *store, const LinkletEdgeId edge_id,
                                     const LinkletBson *bson, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    return store && linklet_object_file_update(&store->edges, edge_id, bson, error);
}

bool linklet_graph_store_delete_edge(LinkletGraphStore *store, const LinkletEdgeId edge_id,
                                     LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!store || edge_id >= store->connectivity_header->edge_count ||
        !linklet_object_file_exists(&store->edges, edge_id)) {
        set_error(error, "edge ID does not exist");
        return false;
    }
    if (!linklet_object_file_delete(&store->edges, edge_id, error)) {
        return false;
    }
    store->coo_edges[edge_id] =
        (LinkletCooEdge){.source_id = UINT64_MAX, .destination_id = UINT64_MAX};
    return true;
}

bool linklet_graph_store_delete_node(LinkletGraphStore *store, const LinkletNodeId node_id,
                                     const bool detach, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!store || !linklet_object_file_exists(&store->nodes, node_id)) {
        set_error(error, "node ID does not exist");
        return false;
    }
    for (uint64_t edge_id = 0; edge_id < store->connectivity_header->edge_count; ++edge_id) {
        const LinkletCooEdge edge = store->coo_edges[edge_id];
        if (linklet_coo_edge_is_deleted(edge) ||
            (edge.source_id != node_id && edge.destination_id != node_id)) {
            continue;
        }
        if (!detach) {
            set_error(error, "node has incident edges; use DETACH DELETE");
            return false;
        }
    }
    if (detach) {
        for (uint64_t edge_id = 0; edge_id < store->connectivity_header->edge_count; ++edge_id) {
            const LinkletCooEdge edge = store->coo_edges[edge_id];
            if (!linklet_coo_edge_is_deleted(edge) &&
                (edge.source_id == node_id || edge.destination_id == node_id) &&
                !linklet_graph_store_delete_edge(store, edge_id, error)) {
                return false;
            }
        }
    }
    return linklet_object_file_delete(&store->nodes, node_id, error);
}

static bool read_object(const LinkletPagedObjectFile *file, const uint64_t id,
                        LinkletStoredObject *object, LinkletError *error) {
    if (!object) {
        set_error(error, "stored object output is required");
        return false;
    }
    *object = (LinkletStoredObject){0};
    object->id = id;
    return linklet_object_file_read(file, id, &object->bson, error);
}

bool linklet_graph_store_read_node(const LinkletGraphStore *store, const LinkletNodeId node_id,
                                   LinkletStoredObject *object, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!store) {
        set_error(error, "graph store is required");
        return false;
    }
    return read_object(&store->nodes, node_id, object, error);
}

bool linklet_graph_store_read_edge(const LinkletGraphStore *store, const LinkletEdgeId edge_id,
                                   LinkletStoredObject *object, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!store) {
        set_error(error, "graph store is required");
        return false;
    }
    return read_object(&store->edges, edge_id, object, error);
}

bool linklet_graph_store_node_exists(const LinkletGraphStore *store, const LinkletNodeId node_id) {
    return store && linklet_object_file_exists(&store->nodes, node_id);
}

bool linklet_graph_store_edge_exists(const LinkletGraphStore *store, const LinkletEdgeId edge_id) {
    return store && linklet_object_file_exists(&store->edges, edge_id);
}

bool linklet_graph_store_edge_endpoints(const LinkletGraphStore *store, const LinkletEdgeId edge_id,
                                        LinkletNodeId *source_id, LinkletNodeId *destination_id) {
    if (!linklet_graph_store_edge_exists(store, edge_id)) {
        return false;
    }
    if (source_id) {
        *source_id = store->coo_edges[edge_id].source_id;
    }
    if (destination_id) {
        *destination_id = store->coo_edges[edge_id].destination_id;
    }
    return true;
}

LinkletFlatCoo linklet_graph_store_coo_view(LinkletGraphStore *store) {
    if (!store) {
        return (LinkletFlatCoo){0};
    }
    return (LinkletFlatCoo){
        .edge_count = (size_t)store->connectivity_header->edge_count,
        .edges = store->coo_edges,
        .vertex_id_capacity = (size_t)store->connectivity_header->vertex_id_capacity,
    };
}

size_t linklet_graph_store_node_count(const LinkletGraphStore *store) {
    return store ? (size_t)store->nodes.header.live_count : 0;
}

size_t linklet_graph_store_edge_count(const LinkletGraphStore *store) {
    return store ? (size_t)store->edges.header.live_count : 0;
}

size_t linklet_graph_store_node_id_capacity(const LinkletGraphStore *store) {
    return store ? (size_t)store->nodes.header.next_id : 0;
}

size_t linklet_graph_store_edge_id_capacity(const LinkletGraphStore *store) {
    return store ? (size_t)store->edges.header.next_id : 0;
}

void linklet_stored_object_destroy(LinkletStoredObject *object) {
    if (!object) {
        return;
    }
    linklet_bson_destroy(&object->bson);
    *object = (LinkletStoredObject){0};
}
