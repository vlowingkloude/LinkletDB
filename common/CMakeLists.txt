add_library(
        common
        linklet_types.c
)

target_include_directories(
        common
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/common
)
