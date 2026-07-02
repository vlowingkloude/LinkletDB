#ifndef LINKLET_NAIVE_TESTS_H
#define LINKLET_NAIVE_TESTS_H

#include <stdbool.h>
#include <stdlib.h>

#define ADD_TEST(func) { #func, func }

typedef struct {
    const char* func_name;
    bool (*func)(void);
} NaiveTestFuncWithZeroArgs;

#endif //LINKLET_NAIVE_TESTS_H
