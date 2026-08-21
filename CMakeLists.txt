cmake_minimum_required(VERSION 4.2)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

project(linklet C)

OPTION(RUN_NAIVE_TESTS "run the main function in naive tests" OFF)

add_subdirectory(common)
add_subdirectory(bson)
add_subdirectory(storage)
add_subdirectory(parser)
add_subdirectory(execution)
add_subdirectory(planner)
add_subdirectory(database)

if (RUN_NAIVE_TESTS)
    enable_testing()
    add_subdirectory(tests/naive_tests)
else ()
    add_subdirectory(shell)
endif ()
