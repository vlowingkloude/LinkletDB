#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "database.h"
#include "naive_tests.h"

static void remove_test_graph(const char *path) {
    const int directory_fd = open(path, O_RDONLY | O_DIRECTORY);
    if (directory_fd >= 0) {
        unlinkat(directory_fd, "connectivity.ll", 0);
        unlinkat(directory_fd, "nodes.ll", 0);
        unlinkat(directory_fd, "edges.ll", 0);
        unlinkat(directory_fd, "metadata.ll", 0);
        unlinkat(directory_fd, "wal.ll", 0);
        close(directory_fd);
    }
    rmdir(path);
}

bool test_catalog_ddl_declares_labels(void) {
    char path[] = "/tmp/linklet-catalog-XXXXXX";
    if (!mkdtemp(path) || rmdir(path) != 0) {
        return false;
    }

    LinkletError error;
    LinkletDatabase *database = linklet_database_create(path, "social", 4, &error);
    if (!database) {
        return false;
    }

    LinkletResult result;
    bool ok = linklet_database_execute(
        database, "CREATE GRAPH TYPE g { (City {name STRING}), (User {name STRING}) }", &result,
        &error);
    linklet_result_destroy(&result);
    const LinkletCatalog *catalog = linklet_database_catalog(database);
    ok = ok && catalog && strcmp(catalog->name, "social") == 0 &&
         linklet_catalog_has_node_label(catalog, "City") &&
         linklet_catalog_has_node_label(catalog, "User") &&
         !linklet_catalog_has_edge_label(catalog, "City");

    ok = ok &&
         linklet_database_execute(database, "CREATE GRAPH TYPE h { EDGE KNOWS }", &result, &error);
    linklet_result_destroy(&result);
    catalog = linklet_database_catalog(database);
    ok = ok && linklet_catalog_has_edge_label(catalog, "KNOWS");

    ok = ok && linklet_database_close(database, &error);
    database = NULL;
    if (!ok) {
        remove_test_graph(path);
        return false;
    }

    database = linklet_database_open(path, &error);
    if (!database) {
        remove_test_graph(path);
        return false;
    }
    catalog = linklet_database_catalog(database);
    ok = strcmp(catalog->name, "social") == 0 && linklet_catalog_has_node_label(catalog, "User") &&
         linklet_catalog_has_edge_label(catalog, "KNOWS");
    ok = linklet_database_close(database, &error) && ok;
    remove_test_graph(path);
    return ok;
}
