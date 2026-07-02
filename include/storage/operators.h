#ifndef LINKLET_OPERATORS_H
#define LINKLET_OPERATORS_H

bool operator_is_reachable_coo_char_array(
    const PagedUnweighedCooArrayType * coo, uint64_t src_id, uint64_t dst_id,
    unsigned char *vector, unsigned char *buffer, int n_hops );

#endif //LINKLET_OPERATORS_H
