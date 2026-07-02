#include <stdlib.h>
#include <string.h>

#include "naive_tests.h"

#include "physical_coo.h"
#include "operators.h"

bool test_is_reachable_coo_char_array() {
    UnweightedCooType edges[] = {
        {.src_id = 1, .dst_id = 2},
        {.src_id = 2, .dst_id = 3},
        {.src_id = 3, .dst_id = 4},
        {.src_id = 2, .dst_id = 4},
        {.src_id = 3, .dst_id = 5},
        {.src_id = 6, .dst_id = 7},
        {.src_id = 7, .dst_id = 8},
        {.src_id = 5, .dst_id = 7}
    };
    const size_t n_items = sizeof(edges) / sizeof(edges[0]);
    const size_t n_nodes = 10;
    unsigned char *vector = malloc(n_nodes * sizeof(unsigned char));
    unsigned char *buffer = malloc(n_nodes * sizeof(unsigned char));
    UnweightedCooPageType pages[2] = {0};
    for (size_t n_pages = 1; n_pages <= 2; n_pages++) {
        // setup page
        PagedUnweighedCooArrayType coo;
        coo.n_pages = n_pages;
        coo.pages = pages;
        UnweightedCooPageType page;
        switch (n_pages) {
            case 1:
                page.n_items = n_items;
                page.coo_array = edges;
                coo.pages[0] = page;
                break;
            case 2:
                page.n_items = 2;
                page.coo_array = edges;
                coo.pages[0] = page;
                UnweightedCooPageType page2;
                page2.n_items = n_items - 2;
                page2.coo_array = edges + 2;
                coo.pages[1] = page2;
                break;
            default:
                return false;
        }

        memset(vector, 0, n_nodes);
        memset(buffer, 0, n_nodes);
        if (operator_is_reachable_coo_char_array(&coo, 1, 2, vector, buffer, 1) == false) {
            return false;
        }
        memset(vector, 0, n_nodes);
        memset(buffer, 0, n_nodes);
        if (operator_is_reachable_coo_char_array(&coo, 1, 2, vector, buffer, 2) == false) {
            return false;
        }
        memset(vector, 0, n_nodes);
        memset(buffer, 0, n_nodes);
        if (operator_is_reachable_coo_char_array(&coo, 1, 4, vector, buffer, 1) == true) {
            return false;
        }
        memset(vector, 0, n_nodes);
        memset(buffer, 0, n_nodes);
        if (operator_is_reachable_coo_char_array(&coo, 1, 4, vector, buffer, 1) == true) {
            return false;
        }
        memset(vector, 0, n_nodes);
        memset(buffer, 0, n_nodes);
        if (operator_is_reachable_coo_char_array(&coo, 1, 4, vector, buffer, 2) == false) {
            return false;
        }
        memset(vector, 0, n_nodes);
        memset(buffer, 0, n_nodes);
        if (operator_is_reachable_coo_char_array(&coo, 7, 1, vector, buffer, -1) == true) {
            return false;
        }
        memset(vector, 0, n_nodes);
        memset(buffer, 0, n_nodes);
        if (operator_is_reachable_coo_char_array(&coo, 1, 7, vector, buffer, -1) == false) {
            return false;
        }
    }
    free(vector);
    free(buffer);
    return true;
}