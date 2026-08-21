add_library(
        database
        database.c
        catalog.c
        wal.c
        transaction.c
)

target_include_directories(
        database
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/database
)

target_link_libraries(
        database
        PUBLIC
        parser
        planner
        execution
)
