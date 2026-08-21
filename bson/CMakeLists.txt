add_library(
        bson
        bson.c
        bson_writer.c
        bson_reader.c
)

target_include_directories(
        bson
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/bson
)
