#ifndef LINKLET_TRANSACTION_H
#define LINKLET_TRANSACTION_H

#include <stdbool.h>

#include "database.h"
#include "kernel.h"

typedef struct LinkletTransaction {
    LinkletDatabase *database;
    LinkletKernelCall *calls;
    size_t call_count;
    size_t capacity;
    bool active;
} LinkletTransaction;

bool linklet_database_begin(LinkletDatabase *database, LinkletTransaction *transaction,
                            LinkletError *error);
bool linklet_transaction_execute(LinkletTransaction *transaction, const char *gql,
                                 LinkletError *error);
bool linklet_transaction_commit(LinkletTransaction *transaction, LinkletError *error);
void linklet_transaction_abort(LinkletTransaction *transaction);

#endif
