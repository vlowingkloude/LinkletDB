add_library(
        storage
        operators/is_reachable_coo_char_array.c
)

target_include_directories(
        storage
        PUBLIC
        ${PROJECT_SOURCE_DIR}/include/storage
)