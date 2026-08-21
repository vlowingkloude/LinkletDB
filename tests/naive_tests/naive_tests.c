#include <stdio.h>

#include "naive_tests.h"

#define NAIVE_SMOKE_TESTS(X)                                                                       \
    X(test_graph_store_persists_flat_coo_and_paged_bson_objects)                                   \
    X(test_parser_create_graph_shape)                                                              \
    X(test_parser_query_shape)                                                                     \
    X(test_parser_errors)                                                                          \
    X(test_simple_parser_match_edge)                                                               \
    X(test_bind_match_node_pattern)                                                                \
    X(test_bind_match_edge_pattern)                                                                \
    X(test_bind_reachability)                                                                      \
    X(test_bind_insert_update_delete)                                                              \
    X(test_bind_rejects_undefined_return_binding)                                                  \
    X(test_operator_reachability_bounded)                                                          \
    X(test_advisor_rejects_out_of_domain_endpoint)                                                 \
    X(test_gql_cud_node_and_edge_match_roundtrip)                                                  \
    X(test_catalog_ddl_declares_labels)                                                            \
    X(test_transaction_commit_and_abort)                                                           \
    X(test_wal_replay_recovers)                                                                    \
    X(test_bson_scalar_and_nested_roundtrip)                                                       \
    X(test_bson_validation_rejects_malformed)                                                      \
    X(test_bson_oid_and_timestamp_roundtrip)

#define DECLARE_NAIVE_TEST_WITH_ZERO_ARGS(function) bool function(void);
NAIVE_SMOKE_TESTS(DECLARE_NAIVE_TEST_WITH_ZERO_ARGS)

#define ADD_TEST(function) {#function, function},
static LinkletNaiveTest naive_smoke_tests[] = {NAIVE_SMOKE_TESTS(ADD_TEST)};

int main() {
    size_t passed_count = 0, test_count = 0;
    for (size_t index = 0; index < sizeof(naive_smoke_tests) / sizeof(naive_smoke_tests[0]);
         ++index) {
        if (!naive_smoke_tests[index].function()) {
            fprintf(stderr, "test %s failed\n", naive_smoke_tests[index].function_name);
        } else {
            passed_count++;
        }
        test_count++;
    }
    printf("Passed %zu / %zu tests\n", passed_count, test_count);
    return passed_count != test_count;
}
