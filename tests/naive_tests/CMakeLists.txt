add_executable(
        linklet_naive_tests
        naive_tests.c
        storage/naive_test_graph_store.c
        parser/naive_test_parser.c
        planner/naive_test_binder.c
        execution/naive_test_flat_coo_execution.c
        execution/naive_test_mutation_execution.c
        database/naive_test_catalog.c
        database/naive_test_transaction.c
        bson/naive_test_bson.c
)

target_include_directories(
        linklet_naive_tests
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/
)

target_link_libraries(
        linklet_naive_tests
        PRIVATE
        database
        execution
        storage
        parser
        planner
        bson
        common
)

add_test(NAME linklet_naive_tests COMMAND linklet_naive_tests)
