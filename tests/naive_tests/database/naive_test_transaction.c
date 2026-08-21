#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bson_reader.h"
#include "bson_writer.h"
#include "database.h"
#include "graph_store.h"
#include "naive_tests.h"
#include "transaction.h"
#include "wal.h"

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

bool test_transaction_commit_and_abort(void) {
    char path[] = "/tmp/linklet-transaction-XXXXXX";
    if (!mkdtemp(path) || rmdir(path) != 0) {
        return false;
    }

    LinkletError error;
    LinkletDatabase *database = linklet_database_create(path, "g", 4, &error);
    if (!database) {
        return false;
    }

    LinkletResult result;
    bool ok = linklet_database_execute(database, "INSERT (a:User {name: 'A'})", &result, &error) &&
              result.inserted_id == 0;
    linklet_result_destroy(&result);

    LinkletTransaction transaction;
    ok = ok && linklet_database_begin(database, &transaction, &error);
    ok = ok && linklet_transaction_execute(&transaction, "INSERT (b:User {name: 'B'})", &error);
    ok =
        ok && linklet_transaction_execute(
                  &transaction, "MATCH (a {id: 0}), (b {id: 1}) INSERT (a)-[e:KNOWS]->(b)", &error);
    ok = ok && linklet_transaction_commit(&transaction, &error);
    ok = ok && linklet_graph_store_node_count(database->store) == 2 &&
         linklet_graph_store_edge_count(database->store) == 1;

    ok = ok && linklet_database_begin(database, &transaction, &error);
    ok = ok && linklet_transaction_execute(&transaction, "INSERT (c:User {name: 'C'})", &error);
    linklet_transaction_abort(&transaction);
    ok = ok && linklet_graph_store_node_count(database->store) == 2;

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
    ok = linklet_graph_store_node_count(database->store) == 2 &&
         linklet_graph_store_edge_count(database->store) == 1;
    ok = linklet_database_close(database, &error) && ok;
    remove_test_graph(path);
    return ok;
}

bool test_wal_replay_recovers(void) {
    char path[] = "/tmp/linklet-wal-XXXXXX";
    if (!mkdtemp(path) || rmdir(path) != 0) {
        return false;
    }

    LinkletError error;
    LinkletDatabase *database = linklet_database_create(path, "g", 4, &error);
    if (!database) {
        return false;
    }
    linklet_database_close(database, &error);

    LinkletBsonError bson_error;
    LinkletBson payload;
    linklet_bson_init(&payload);
    LinkletBson labels;
    linklet_bson_init(&labels);
    linklet_bson_array_append_utf8(&labels, "User", -1, &bson_error);
    linklet_bson_append_array(&payload, "_labels", &labels, &bson_error);
    linklet_bson_append_utf8(&payload, "name", "Recovered", -1, &bson_error);
    linklet_bson_destroy(&labels);

    char wal_file[512];
    snprintf(wal_file, sizeof(wal_file), "%s/wal.ll", path);
    LinkletMmapFile *wal = linklet_wal_open(wal_file, false, &error);
    bool ok = wal != NULL;
    if (ok) {
        const LinkletResolvedOperation operation = {
            .operation = LINKLET_WAL_INSERT_NODE,
            .id = 0,
            .payload = &payload,
        };
        ok = linklet_wal_write_frame(wal, 1, &operation, 1, &error);
        linklet_wal_close(wal);
    }
    linklet_bson_destroy(&payload);
    if (!ok) {
        remove_test_graph(path);
        return false;
    }

    database = linklet_database_open(path, &error);
    if (!database) {
        remove_test_graph(path);
        return false;
    }
    ok = linklet_graph_store_node_count(database->store) == 1;
    LinkletStoredObject object = {0};
    const char *name = NULL;
    ok = ok && linklet_graph_store_read_node(database->store, 0, &object, &error) &&
         linklet_bson_get_utf8(&object.bson, "name", &name, NULL) && strcmp(name, "Recovered") == 0;
    linklet_stored_object_destroy(&object);
    ok = linklet_database_close(database, &error) && ok;
    remove_test_graph(path);
    return ok;
}
