add_library(
        planner
        binder.c
        bson_ast.c
)

target_include_directories(
        planner
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/planner
)

target_link_libraries(
        planner
        PUBLIC
        parser
        execution
)
