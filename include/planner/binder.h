#ifndef LINKLET_BINDER_H
#define LINKLET_BINDER_H

#include <stdbool.h>

#include "gql_ast.h"
#include "kernel.h"

bool linklet_bind(const GqlNode *ast, LinkletLogicalPlan *plan, LinkletError *error);

#endif
