#include <stdio.h>

#include "naive_tests.h"

bool test_is_reachable_coo_char_array();

static NaiveTestFuncWithZeroArgs tests[] = {
    ADD_TEST(test_is_reachable_coo_char_array),
};

int main() {
    size_t n_passed_functions = 0, n_tests = 0;
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        if (!tests[i].func()) {
            fprintf(stderr, "test %s failed\n", tests[i].func_name);
        } else {
            n_passed_functions++;
        }
        n_tests++;
    }
    printf("Passed %zu / %zu tests\n", n_passed_functions, n_tests);
    return n_passed_functions != n_tests;
}