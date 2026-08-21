#ifndef LINKLET_GRAPH_STORE_H
#define LINKLET_GRAPH_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bson.h"
#include "linklet_types.h"
#include "physical_coo.h"

typedef LinkletId LinkletNodeId;
typedef LinkletId LinkletEdgeId;

typedef struct LinkletGraphStore LinkletGraphStore;

typedef struct LinkletStoredObject {
    uint64_t id;
    LinkletBson bson;
} LinkletStoredObject;

LinkletGraphStore *linklet_graph_store_create(const char *path, size_t initial_edge_capacity,
                                              LinkletError *error);
LinkletGraphStore *linklet_graph_store_open(const char *path, LinkletError *error);
bool linklet_graph_store_close(LinkletGraphStore *store, LinkletError *error);

bool linklet_graph_store_insert_node(LinkletGraphStore *store, const LinkletBson *bson,
                                     LinkletNodeId *node_id, LinkletError *error);
bool linklet_graph_store_insert_edge(LinkletGraphStore *store, LinkletNodeId source_id,
                                     LinkletNodeId destination_id, const LinkletBson *bson,
                                     LinkletEdgeId *edge_id, LinkletError *error);
bool linklet_graph_store_update_node(LinkletGraphStore *store, LinkletNodeId node_id,
                                     const LinkletBson *bson, LinkletError *error);
bool linklet_graph_store_update_edge(LinkletGraphStore *store, LinkletEdgeId edge_id,
                                     const LinkletBson *bson, LinkletError *error);
bool linklet_graph_store_delete_node(LinkletGraphStore *store, LinkletNodeId node_id, bool detach,
                                     LinkletError *error);
bool linklet_graph_store_delete_edge(LinkletGraphStore *store, LinkletEdgeId edge_id,
                                     LinkletError *error);

bool linklet_graph_store_read_node(const LinkletGraphStore *store, LinkletNodeId node_id,
                                   LinkletStoredObject *object, LinkletError *error);
bool linklet_graph_store_read_edge(const LinkletGraphStore *store, LinkletEdgeId edge_id,
                                   LinkletStoredObject *object, LinkletError *error);

bool linklet_graph_store_node_exists(const LinkletGraphStore *store, LinkletNodeId node_id);
bool linklet_graph_store_edge_exists(const LinkletGraphStore *store, LinkletEdgeId edge_id);
bool linklet_graph_store_edge_endpoints(const LinkletGraphStore *store, LinkletEdgeId edge_id,
                                        LinkletNodeId *source_id, LinkletNodeId *destination_id);

LinkletFlatCoo linklet_graph_store_coo_view(LinkletGraphStore *store);
size_t linklet_graph_store_node_count(const LinkletGraphStore *store);
size_t linklet_graph_store_edge_count(const LinkletGraphStore *store);
size_t linklet_graph_store_node_id_capacity(const LinkletGraphStore *store);
size_t linklet_graph_store_edge_id_capacity(const LinkletGraphStore *store);

void linklet_stored_object_destroy(LinkletStoredObject *object);

#endif
