#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "bson_reader.h"
#include "bson_writer.h"
#include "graph_store.h"
#include "naive_tests.h"

static void remove_test_graph(const char *path) {
    const int directory_fd = open(path, O_RDONLY | O_DIRECTORY);
    if (directory_fd >= 0) {
        unlinkat(directory_fd, "connectivity.ll", 0);
        unlinkat(directory_fd, "nodes.ll", 0);
        unlinkat(directory_fd, "edges.ll", 0);
        close(directory_fd);
    }
    rmdir(path);
}

static bool make_object(const char *label, const char *name, LinkletBson *bson) {
    LinkletBsonError error;
    LinkletBson labels;
    linklet_bson_init(&labels);
    linklet_bson_init(bson);
    const bool success = linklet_bson_array_append_utf8(&labels, label, -1, &error) &&
                         linklet_bson_append_array(bson, "_labels", &labels, &error) &&
                         linklet_bson_append_utf8(bson, "name", name, -1, &error);
    linklet_bson_destroy(&labels);
    if (!success) {
        linklet_bson_destroy(bson);
    }
    return success;
}

bool test_graph_store_persists_flat_coo_and_paged_bson_objects(void) {
    char path[] = "/tmp/linklet-store-XXXXXX";
    if (!mkdtemp(path) || rmdir(path) != 0) {
        return false;
    }

    LinkletError error;
    LinkletGraphStore *store = linklet_graph_store_create(path, 2, &error);
    if (!store) {
        return false;
    }

    bool valid = true;
    char name[64];
    for (size_t index = 0; valid && index < 1400; ++index) {
        snprintf(name, sizeof(name), "user-%zu", index);
        LinkletBson bson;
        valid = make_object("User", name, &bson);
        LinkletNodeId id = UINT64_MAX;
        valid = valid && linklet_graph_store_insert_node(store, &bson, &id, &error) && id == index;
        linklet_bson_destroy(&bson);
    }

    LinkletBson edge_bson;
    valid = valid && make_object("KNOWS", "friendship", &edge_bson);
    static const LinkletCooEdge expected_edges[] = {
        {.source_id = 0, .destination_id = 1},
        {.source_id = 1, .destination_id = 2},
        {.source_id = 2, .destination_id = 3},
    };
    for (size_t index = 0; valid && index < 3; ++index) {
        LinkletEdgeId id = UINT64_MAX;
        valid = linklet_graph_store_insert_edge(store, expected_edges[index].source_id,
                                                expected_edges[index].destination_id, &edge_bson,
                                                &id, &error) &&
                id == index;
    }
    linklet_bson_destroy(&edge_bson);

    char large_value[4096];
    memset(large_value, 'x', sizeof(large_value) - 1);
    large_value[sizeof(large_value) - 1] = '\0';
    LinkletBson replacement;
    valid = valid && make_object("User", large_value, &replacement) &&
            linklet_graph_store_update_node(store, 1, &replacement, &error);
    linklet_bson_destroy(&replacement);
    valid = valid && linklet_graph_store_delete_edge(store, 1, &error) &&
            linklet_graph_store_node_count(store) == 1400 &&
            linklet_graph_store_edge_count(store) == 2;

    valid = valid && linklet_graph_store_close(store, &error);
    store = NULL;
    if (!valid) {
        remove_test_graph(path);
        return false;
    }

    char nodes_path[512];
    snprintf(nodes_path, sizeof(nodes_path), "%s/nodes.ll", path);
    struct stat file_info;
    valid = stat(nodes_path, &file_info) == 0 && file_info.st_size > 64 + 64 * 1024;

    store = linklet_graph_store_open(path, &error);
    if (!store) {
        remove_test_graph(path);
        return false;
    }
    const LinkletFlatCoo coo = linklet_graph_store_coo_view(store);
    valid = valid && coo.edge_count == 3 && coo.vertex_id_capacity == 1400 &&
            coo.edges[0].source_id == 0 && coo.edges[0].destination_id == 1 &&
            linklet_coo_edge_is_deleted(coo.edges[1]) && coo.edges[2].source_id == 2 &&
            coo.edges[2].destination_id == 3;

    LinkletStoredObject node = {0};
    const char *stored_name = NULL;
    size_t stored_name_size = 0;
    valid = valid && linklet_graph_store_read_node(store, 1, &node, &error) &&
            linklet_bson_get_utf8(&node.bson, "name", &stored_name, &stored_name_size) &&
            stored_name_size == strlen(large_value) &&
            memcmp(stored_name, large_value, stored_name_size) == 0 &&
            !linklet_graph_store_read_edge(store, 1, &(LinkletStoredObject){0}, &error);

    linklet_stored_object_destroy(&node);
    valid = linklet_graph_store_close(store, &error) && valid;
    remove_test_graph(path);
    return valid;
}
