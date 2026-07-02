add_executable(
        linklet_naive_tests
        naive_tests.c
        storage/operators/naive_test_is_reachable_coo_char_array.c
)

target_include_directories(
        linklet_naive_tests
        PUBLIC
        ${PROJECT_SOURCE_DIR}/tests/naive_tests
)

target_link_libraries(
        linklet_naive_tests
        PRIVATE
        storage
)