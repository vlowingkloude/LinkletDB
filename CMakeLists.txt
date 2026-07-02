cmake_minimum_required(VERSION 4.2)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

project(linklet C)

OPTION(RUN_NAIVE_TESTS "run the main function in naive tests" ON)

add_subdirectory(storage)

if (RUN_NAIVE_TESTS)
    add_subdirectory(tests/naive_tests)
endif ()