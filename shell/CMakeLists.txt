OPTION(LINKLET_DEBUG_SHELL "enable '?plan' dump in linklet_shell" ON)

add_executable(linklet_shell linklet_shell.c)
target_link_libraries(linklet_shell PRIVATE database)

add_executable(gql_ast_dump gql_ast_dump.c)
target_link_libraries(gql_ast_dump PRIVATE parser)

if (LINKLET_DEBUG_SHELL)
    target_compile_definitions(linklet_shell PRIVATE LINKLET_DEBUG)
endif()
