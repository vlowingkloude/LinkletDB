add_library(
        parser
        ast.c
        lexer.c
        parser.c
)

target_include_directories(
        parser
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/parser
)
