#ifndef LINKLETDB_PHYSICAL_COO_H
#define LINKLETDB_PHYSICAL_COO_H

#include <stdint.h>
#include <stdlib.h>

typedef struct {
    uint64_t src_id;
    uint64_t dst_id;
} UnweightedCooType;

typedef struct {
    size_t n_items;
    UnweightedCooType *coo_array;
} UnweightedCooPageType;

typedef struct {
    size_t n_pages;
    UnweightedCooPageType *pages;
} PagedUnweighedCooArrayType;

#endif //LINKLETDB_PHYSICAL_COO_H
