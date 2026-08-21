#ifndef LINKLET_DATABASE_H
#define LINKLET_DATABASE_H

#include <stdbool.h>

#include "catalog.h"
#include "executor.h"
#include "graph_store.h"

typedef struct LinkletDatabase {
    LinkletGraphStore *store;
    char *path;
    LinkletCatalog catalog;
} LinkletDatabase;

LinkletDatabase *linklet_database_create(const char *path, const char *graph_name,
                                         size_t initial_edge_capacity, LinkletError *error);
LinkletDatabase *linklet_database_open(const char *path, LinkletError *error);
bool linklet_database_close(LinkletDatabase *database, LinkletError *error);

bool linklet_database_execute(LinkletDatabase *database, const char *gql, LinkletResult *result,
                              LinkletError *error);

const LinkletCatalog *linklet_database_catalog(const LinkletDatabase *database);

#endif
