#ifndef LINKLET_CATALOG_H
#define LINKLET_CATALOG_H

#include <stdbool.h>

#include "bson.h"
#include "gql_ast.h"
#include "linklet_types.h"

typedef struct LinkletCatalog {
    char *name;
    char **node_labels;
    size_t node_label_count;
    size_t node_label_capacity;
    char **edge_labels;
    size_t edge_label_count;
    size_t edge_label_capacity;
} LinkletCatalog;

void linklet_catalog_init(LinkletCatalog *catalog);
void linklet_catalog_destroy(LinkletCatalog *catalog);

bool linklet_catalog_add_node_label(LinkletCatalog *catalog, const char *label);
bool linklet_catalog_add_edge_label(LinkletCatalog *catalog, const char *label);
bool linklet_catalog_has_node_label(const LinkletCatalog *catalog, const char *label);
bool linklet_catalog_has_edge_label(const LinkletCatalog *catalog, const char *label);

bool linklet_catalog_to_bson(const LinkletCatalog *catalog, LinkletBson *out, LinkletError *error);
bool linklet_catalog_from_bson(LinkletCatalog *catalog, const LinkletBson *doc,
                               LinkletError *error);

bool linklet_is_catalog_ddl(const GqlNode *ast);

bool linklet_catalog_apply_ddl(LinkletCatalog *catalog, const GqlNode *ast, LinkletError *error);

#endif
