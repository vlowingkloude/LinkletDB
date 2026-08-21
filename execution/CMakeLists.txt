add_library(
        execution
        advisor.c
        executor.c
        kernel.c
        operators.c
)

target_include_directories(
        execution
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/execution
)

target_link_libraries(
        execution
        PUBLIC
        common
        storage
        bson
)
