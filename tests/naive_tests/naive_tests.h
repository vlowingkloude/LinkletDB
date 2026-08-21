#ifndef LINKLET_NAIVE_TESTS_H
#define LINKLET_NAIVE_TESTS_H

#include <stdbool.h>

typedef struct LinkletNaiveTest {
    const char *function_name;
    bool (*function)(void);
} LinkletNaiveTest;

#endif
