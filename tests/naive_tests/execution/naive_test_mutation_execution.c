#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bson_reader.h"
#include "database.h"
#include "graph_store.h"
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

static bool execute_insert(LinkletDatabase *database, const char *text,
                           const uint64_t expected_id) {
    LinkletResult result;
    LinkletError error;
    const bool success = linklet_database_execute(database, text, &result, &error) &&
                         result.kind == LINKLET_RESULT_COUNT && result.inserted_id == expected_id &&
                         result.affected_count == 1;
    linklet_result_destroy(&result);
    return success;
}

static bool execute_change(LinkletDatabase *database, const char *text,
                           const size_t expected_affected) {
    LinkletResult result;
    LinkletError error;
    const bool success = linklet_database_execute(database, text, &result, &error) &&
                         result.kind == LINKLET_RESULT_COUNT &&
                         result.affected_count == expected_affected;
    linklet_result_destroy(&result);
    return success;
}

static bool match_one(LinkletDatabase *database, const char *text, const LinkletElementKind kind,
                      const uint64_t expected_id) {
    LinkletResult result;
    LinkletError error;
    const bool success = linklet_database_execute(database, text, &result, &error) &&
                         result.kind == LINKLET_RESULT_IDS && result.element_kind == kind &&
                         result.ids.count == 1 && result.ids.ids[0] == expected_id;
    linklet_result_destroy(&result);
    return success;
}

static bool match_none(LinkletDatabase *database, const char *text) {
    LinkletResult result;
    LinkletError error;
    const bool success = linklet_database_execute(database, text, &result, &error) &&
                         result.kind == LINKLET_RESULT_IDS && result.ids.count == 0;
    linklet_result_destroy(&result);
    return success;
}

static bool execute_reachability(LinkletDatabase *database, const char *text, bool *reachable) {
    LinkletResult result;
    LinkletError error;
    const bool success = linklet_database_execute(database, text, &result, &error) &&
                         result.kind == LINKLET_RESULT_BOOL;
    if (success) {
        *reachable = result.boolean;
    }
    linklet_result_destroy(&result);
    return success;
}

bool test_gql_cud_node_and_edge_match_roundtrip(void) {
    char path[] = "/tmp/linklet-gql-XXXXXX";
    if (!mkdtemp(path) || rmdir(path) != 0) {
        return false;
    }

    LinkletError error;
    LinkletDatabase *database = linklet_database_create(path, "test-graph", 2, &error);
    if (!database) {
        return false;
    }

    static const char *node_inserts[] = {
        "INSERT (alice:User {name: 'Alice', age: 30})",
        "INSERT (bob:User {name: 'Bob', active: TRUE})",
        "INSERT (carol:User {name: 'Carol'})",
        "INSERT (dora:User {name: 'Dora'})",
    };
    bool valid = true;
    for (size_t node = 0; valid && node < 4; ++node) {
        valid = execute_insert(database, node_inserts[node], node);
    }

    static const char *edge_inserts[] = {
        "MATCH (a {id: 0}), (b {id: 1}) INSERT (a)-[e:KNOWS {since: 2021}]->(b)",
        "MATCH (a {id: 1}), (b {id: 2}) INSERT (a)-[e:KNOWS {since: 2022}]->(b)",
        "MATCH (a {id: 2}), (b {id: 3}) INSERT (a)-[e:KNOWS {since: 2023}]->(b)",
    };
    for (size_t edge = 0; valid && edge < 3; ++edge) {
        valid = execute_insert(database, edge_inserts[edge], edge);
    }

    valid = valid && match_one(database, "MATCH (n:User {name: 'Alice', age: 30}) RETURN n",
                               LINKLET_ELEMENT_NODE, 0);
    valid = valid && match_one(database,
                               "MATCH (a:User {name: 'Alice'})-[e:KNOWS {since: 2021}]->(b:User "
                               "{name: 'Bob'}) RETURN e",
                               LINKLET_ELEMENT_EDGE, 0);

    valid = valid && execute_change(database,
                                    "MATCH (n:User {name: 'Alice'}) SET n.name = 'Alicia', n.bio = "
                                    "'graph systems researcher' RETURN n",
                                    1);
    valid = valid && match_none(database, "MATCH (n:User {name: 'Alice'}) RETURN n") &&
            match_one(database, "MATCH (n:User {name: 'Alicia', age: 30}) RETURN n",
                      LINKLET_ELEMENT_NODE, 0);

    valid =
        valid &&
        execute_change(
            database,
            "MATCH (a {id: 0})-[e:KNOWS {since: 2021}]->(b {id: 1}) SET e.strength = 0.75 RETURN e",
            1) &&
        match_one(database,
                  "MATCH (a {id: 0})-[e:KNOWS {since: 2021, strength: 0.75}]->(b {id: 1}) RETURN e",
                  LINKLET_ELEMENT_EDGE, 0);

    valid = valid &&
            execute_change(database,
                           "MATCH (a {id: 0})-[e:KNOWS {since: 2021}]->(b {id: 1}) DELETE e", 1);
    bool reachable = true;
    valid = valid &&
            execute_reachability(database, "MATCH (a {id: 0})-[e]->{1,3}(b {id: 3}) RETURN b",
                                 &reachable) &&
            !reachable;

    LinkletResult rejected_result;
    LinkletError rejected_error;
    valid = valid &&
            !linklet_database_execute(database, "MATCH (n {id: 1}) DELETE n", &rejected_result,
                                      &rejected_error) &&
            linklet_graph_store_node_exists(database->store, 1);
    linklet_result_destroy(&rejected_result);
    valid = valid && execute_change(database, "MATCH (n {id: 1}) DETACH DELETE n", 1) &&
            !linklet_graph_store_node_exists(database->store, 1) &&
            !linklet_graph_store_edge_exists(database->store, 1) &&
            linklet_graph_store_node_count(database->store) == 3 &&
            linklet_graph_store_edge_count(database->store) == 1;

    valid = valid && linklet_database_close(database, &error);
    database = NULL;
    if (!valid) {
        remove_test_graph(path);
        return false;
    }

    database = linklet_database_open(path, &error);
    if (!database) {
        remove_test_graph(path);
        return false;
    }
    LinkletStoredObject alicia = {0};
    const char *name = NULL;
    valid = linklet_graph_store_read_node(database->store, 0, &alicia, &error) &&
            linklet_bson_get_utf8(&alicia.bson, "name", &name, NULL) &&
            strcmp(name, "Alicia") == 0 &&
            match_one(database,
                      "MATCH (a:User {name: 'Carol'})-[e:KNOWS {since: 2023}]->(b:User {name: "
                      "'Dora'}) RETURN e",
                      LINKLET_ELEMENT_EDGE, 2) &&
            match_none(database, "MATCH (n:User {name: 'Bob'}) RETURN n");

    linklet_stored_object_destroy(&alicia);
    valid = linklet_database_close(database, &error) && valid;
    remove_test_graph(path);
    return valid;
}
