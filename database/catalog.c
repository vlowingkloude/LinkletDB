#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "catalog.h"
#include "bson_reader.h"
#include "bson_writer.h"

enum {
    LINKLET_CATALOG_INITIAL_LABEL_CAPACITY = 8,
    LINKLET_CATALOG_CAPACITY_GROWTH_FACTOR = 2,
};

static void set_error(LinkletError *error, const char *format, ...) {
    if (!error) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
}

static char *duplicate(const char *text) {
    if (!text) {
        return NULL;
    }
    const size_t size = strlen(text) + 1;
    char *copy = (char *)malloc(size);
    if (copy) {
        memcpy(copy, text, size);
    }
    return copy;
}

static const GqlNode *find_first(const GqlNode *node, const GqlNodeKind kind) {
    if (!node) {
        return NULL;
    }
    if (node->kind == kind) {
        return node;
    }
    for (size_t child = 0; child < node->child_count; ++child) {
        const GqlNode *found = find_first(node->children[child], kind);
        if (found) {
            return found;
        }
    }
    return NULL;
}

static const GqlNode *direct_child(const GqlNode *node, const GqlNodeKind kind) {
    if (!node) {
        return NULL;
    }
    for (size_t child = 0; child < node->child_count; ++child) {
        if (node->children[child]->kind == kind) {
            return node->children[child];
        }
    }
    return NULL;
}

void linklet_catalog_init(LinkletCatalog *catalog) {
    if (catalog) {
        *catalog = (LinkletCatalog){0};
    }
}

void linklet_catalog_destroy(LinkletCatalog *catalog) {
    if (!catalog) {
        return;
    }
    free(catalog->name);
    for (size_t index = 0; index < catalog->node_label_count; ++index) {
        free(catalog->node_labels[index]);
    }
    free(catalog->node_labels);
    for (size_t index = 0; index < catalog->edge_label_count; ++index) {
        free(catalog->edge_labels[index]);
    }
    free(catalog->edge_labels);
    *catalog = (LinkletCatalog){0};
}

static bool contains(char **labels, const size_t count, const char *label) {
    for (size_t index = 0; index < count; ++index) {
        if (strcmp(labels[index], label) == 0) {
            return true;
        }
    }
    return false;
}

static bool add_label(char ***labels, size_t *count, size_t *capacity, const char *label) {
    if (contains(*labels, *count, label)) {
        return true;
    }
    if (*count == *capacity) {
        const size_t next = *capacity ? *capacity * LINKLET_CATALOG_CAPACITY_GROWTH_FACTOR
                                      : LINKLET_CATALOG_INITIAL_LABEL_CAPACITY;
        char **grown = (char **)realloc(*labels, next * sizeof(*grown));
        if (!grown) {
            return false;
        }
        *labels = grown;
        *capacity = next;
    }
    char *copy = duplicate(label);
    if (!copy) {
        return false;
    }
    (*labels)[(*count)++] = copy;
    return true;
}

bool linklet_catalog_add_node_label(LinkletCatalog *catalog, const char *label) {
    if (!catalog || !label) {
        return false;
    }
    return add_label(&catalog->node_labels, &catalog->node_label_count,
                     &catalog->node_label_capacity, label);
}

bool linklet_catalog_add_edge_label(LinkletCatalog *catalog, const char *label) {
    if (!catalog || !label) {
        return false;
    }
    return add_label(&catalog->edge_labels, &catalog->edge_label_count,
                     &catalog->edge_label_capacity, label);
}

bool linklet_catalog_has_node_label(const LinkletCatalog *catalog, const char *label) {
    return catalog && label && contains(catalog->node_labels, catalog->node_label_count, label);
}

bool linklet_catalog_has_edge_label(const LinkletCatalog *catalog, const char *label) {
    return catalog && label && contains(catalog->edge_labels, catalog->edge_label_count, label);
}

bool linklet_catalog_to_bson(const LinkletCatalog *catalog, LinkletBson *out, LinkletError *error) {
    if (!catalog || !out) {
        set_error(error, "catalog and output document are required");
        return false;
    }
    linklet_bson_init(out);
    LinkletBsonError bson_error;
    if (!linklet_bson_append_utf8(out, "name", catalog->name ? catalog->name : "", -1,
                                  &bson_error)) {
        set_error(error, "%s", bson_error.message);
        linklet_bson_destroy(out);
        return false;
    }

    LinkletBson nodes;
    linklet_bson_init(&nodes);
    for (size_t index = 0; index < catalog->node_label_count; ++index) {
        if (!linklet_bson_array_append_utf8(&nodes, catalog->node_labels[index], -1, &bson_error)) {
            set_error(error, "%s", bson_error.message);
            linklet_bson_destroy(&nodes);
            linklet_bson_destroy(out);
            return false;
        }
    }
    const bool ok_nodes = linklet_bson_append_array(out, "node_labels", &nodes, &bson_error);
    linklet_bson_destroy(&nodes);

    LinkletBson edges;
    linklet_bson_init(&edges);
    for (size_t index = 0; index < catalog->edge_label_count; ++index) {
        if (!linklet_bson_array_append_utf8(&edges, catalog->edge_labels[index], -1, &bson_error)) {
            set_error(error, "%s", bson_error.message);
            linklet_bson_destroy(&edges);
            linklet_bson_destroy(out);
            return false;
        }
    }
    const bool ok_edges = linklet_bson_append_array(out, "edge_labels", &edges, &bson_error);
    linklet_bson_destroy(&edges);

    if (!ok_nodes || !ok_edges) {
        set_error(error, "%s", bson_error.message);
        linklet_bson_destroy(out);
        return false;
    }
    return true;
}

static bool collect_array(LinkletCatalog *catalog, const LinkletBson *doc, const char *key,
                          const bool is_node, LinkletError *error) {
    const uint8_t *data = NULL;
    size_t size = 0;
    if (!linklet_bson_get_array(doc, key, &data, &size)) {
        return true;
    }
    const LinkletBson array = linklet_bson_view(data, size);
    LinkletBsonIterator iterator;
    if (!linklet_bson_iterator_init(&iterator, &array)) {
        return true;
    }
    do {
        const char *label = NULL;
        if (linklet_bson_iterator_utf8(&iterator, &label, NULL)) {
            const bool ok = is_node ? linklet_catalog_add_node_label(catalog, label)
                                    : linklet_catalog_add_edge_label(catalog, label);
            if (!ok) {
                set_error(error, "out of memory while loading catalog labels");
                return false;
            }
        }
    } while (linklet_bson_iterator_next(&iterator));
    return true;
}

bool linklet_catalog_from_bson(LinkletCatalog *catalog, const LinkletBson *doc,
                               LinkletError *error) {
    if (!catalog || !doc) {
        set_error(error, "catalog and document are required");
        return false;
    }
    linklet_catalog_destroy(catalog);
    linklet_catalog_init(catalog);

    const char *name = NULL;
    linklet_bson_get_utf8(doc, "name", &name, NULL);
    catalog->name = duplicate(name ? name : "");
    if (!catalog->name) {
        set_error(error, "out of memory loading catalog name");
        return false;
    }
    return collect_array(catalog, doc, "node_labels", true, error) &&
           collect_array(catalog, doc, "edge_labels", false, error);
}

bool linklet_is_catalog_ddl(const GqlNode *ast) {
    return find_first(ast, GQL_CREATE_GRAPH) != NULL ||
           find_first(ast, GQL_CREATE_GRAPH_TYPE) != NULL ||
           find_first(ast, GQL_DROP_GRAPH) != NULL || find_first(ast, GQL_DROP_GRAPH_TYPE) != NULL;
}

static bool collect_label_names(const GqlNode *label, char ***labels, size_t *count,
                                size_t *capacity) {
    if (!label) {
        return true;
    }
    if (label->kind == GQL_NAME && label->text) {
        return add_label(labels, count, capacity, label->text);
    }
    for (size_t child = 0; child < label->child_count; ++child) {
        if (!collect_label_names(label->children[child], labels, count, capacity)) {
            return false;
        }
    }
    return true;
}

static bool add_element_labels(LinkletCatalog *catalog, const GqlNode *element, const bool is_node,
                               LinkletError *error) {

    const GqlNode *label_expression = find_first(element, GQL_LABEL_EXPRESSION);
    if (label_expression) {
        char **labels = NULL;
        size_t count = 0, capacity = 0;
        if (!collect_label_names(label_expression, &labels, &count, &capacity)) {
            free(labels);
            set_error(error, "out of memory collecting labels");
            return false;
        }
        for (size_t index = 0; index < count; ++index) {
            const bool ok = is_node ? linklet_catalog_add_node_label(catalog, labels[index])
                                    : linklet_catalog_add_edge_label(catalog, labels[index]);
            if (!ok) {
                for (size_t j = 0; j < count; ++j) {
                    free(labels[j]);
                }
                free(labels);
                set_error(error, "out of memory registering label");
                return false;
            }
        }
        for (size_t j = 0; j < count; ++j) {
            free(labels[j]);
        }
        free(labels);
        return true;
    }

    const char *fallback = element->text;
    if (!fallback) {
        const GqlNode *name = direct_child(element, GQL_NAME);
        fallback = name ? name->text : NULL;
    }
    if (fallback) {
        const bool ok = is_node ? linklet_catalog_add_node_label(catalog, fallback)
                                : linklet_catalog_add_edge_label(catalog, fallback);
        if (!ok) {
            set_error(error, "out of memory registering label");
            return false;
        }
    }
    return true;
}

static bool apply_nested_graph_type(LinkletCatalog *catalog, const GqlNode *nested,
                                    LinkletError *error) {
    const GqlNode *list = direct_child(nested, GQL_ELEMENT_TYPE_LIST);
    if (!list) {
        return true;
    }
    for (size_t index = 0; index < list->child_count; ++index) {
        const GqlNode *element = list->children[index];
        const bool is_node = element->kind == GQL_NODE_TYPE;
        if (element->kind != GQL_NODE_TYPE && element->kind != GQL_EDGE_TYPE) {
            continue;
        }
        if (!add_element_labels(catalog, element, is_node, error)) {
            return false;
        }
    }
    return true;
}

bool linklet_catalog_apply_ddl(LinkletCatalog *catalog, const GqlNode *ast, LinkletError *error) {
    if (error) {
        error->message[0] = '\0';
    }
    if (!catalog || !ast) {
        set_error(error, "catalog and AST are required");
        return false;
    }
    const GqlNode *create_graph_type = find_first(ast, GQL_CREATE_GRAPH_TYPE);
    if (create_graph_type) {
        const GqlNode *nested = find_first(create_graph_type, GQL_NESTED_GRAPH_TYPE);
        if (!nested) {
            set_error(error, "CREATE GRAPH TYPE with a nested graph type is required");
            return false;
        }
        return apply_nested_graph_type(catalog, nested, error);
    }

    const GqlNode *create_graph = find_first(ast, GQL_CREATE_GRAPH);
    if (create_graph) {
        const GqlNode *name = direct_child(create_graph, GQL_NAME);
        if (name && name->text) {
            free(catalog->name);
            catalog->name = duplicate(name->text);
            if (!catalog->name) {
                set_error(error, "out of memory setting graph name");
                return false;
            }
        }
        const GqlNode *nested = find_first(create_graph, GQL_NESTED_GRAPH_TYPE);
        return nested ? apply_nested_graph_type(catalog, nested, error) : true;
    }

    set_error(error, "DROP GRAPH / DROP GRAPH TYPE are not implemented");
    return false;
}
