add_library(
        storage
        graph_store.c
        paged_object_file.c
        mmap_file.c
)

target_include_directories(
        storage
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/storage
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(storage PUBLIC common bson)
