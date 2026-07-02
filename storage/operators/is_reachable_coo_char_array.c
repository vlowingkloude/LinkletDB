#include <stdbool.h>

#include "physical_coo.h"
#include "operators.h"

// for very small graphs with unordered coo
// no bitmap, no parallel
bool operator_is_reachable_coo_char_array(
    const PagedUnweighedCooArrayType *const coo, const uint64_t src_id, const uint64_t dst_id,
    unsigned char *vector, unsigned char *buffer, int n_hops ) {
    // label 1-hop dst
    for (size_t i_page = 0; i_page < coo->n_pages; ++i_page) {
        for (size_t i_item = 0; i_item < coo->pages[i_page].n_items; ++i_item) {
            vector[coo->pages[i_page].coo_array[i_item].dst_id] |= coo->pages[i_page].coo_array[i_item].src_id == src_id;
        }
    }
    bool cond = (n_hops > 1 || n_hops <= -1) && vector[dst_id] == 0;
    while (cond) {
        for (size_t i_page = 0; i_page < coo->n_pages; ++i_page) {
            for (size_t i_item = 0; i_item < coo->pages[i_page].n_items; ++i_item) {
                const unsigned char val = buffer[coo->pages[i_page].coo_array[i_item].dst_id] | vector[coo->pages[i_page].coo_array[i_item].src_id];
                cond = cond && (val == buffer[coo->pages[i_page].coo_array[i_item].dst_id]);
                buffer[coo->pages[i_page].coo_array[i_item].dst_id] = val;
            }
        }
        n_hops--;
        cond = !cond && (n_hops > 1 || n_hops <= -1) && buffer[dst_id] == 0;
        unsigned char *swap = vector;
        vector = buffer;
        buffer = swap;
    }
    return vector[dst_id] != 0;
}