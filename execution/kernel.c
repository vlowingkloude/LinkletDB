#include "kernel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void object_filter_destroy(LinkletObjectFilter *filter) {
    if (!filter) {
        return;
    }
    free(filter->label);
    linklet_bson_destroy(&filter->properties);
    *filter = (LinkletObjectFilter){0};
}

void linklet_kernel_call_destroy(LinkletKernelCall *call) {
    if (!call) {
        return;
    }
    object_filter_destroy(&call->match.source);
    object_filter_destroy(&call->match.edge);
    object_filter_destroy(&call->match.destination);
    linklet_bson_destroy(&call->payload);
    *call = (LinkletKernelCall){0};
}

void linklet_logical_plan_destroy(LinkletLogicalPlan *plan) {
    if (!plan) {
        return;
    }
    for (size_t index = 0; index < plan->call_count; ++index) {
        linklet_kernel_call_destroy(&plan->calls[index]);
    }
    free(plan->calls);
    *plan = (LinkletLogicalPlan){0};
}

static const char *kernel_code_name(const LinkletKernelCode code) {
    switch (code) {
    case LINKLET_KERNEL_MATCH:
        return "match";
    case LINKLET_KERNEL_REACHABILITY:
        return "reachability";
    case LINKLET_KERNEL_INSERT:
        return "insert";
    case LINKLET_KERNEL_UPDATE:
        return "update";
    case LINKLET_KERNEL_DELETE:
        return "delete";
    default:
        return "unknown";
    }
}

static const char *direction_name(const LinkletDirection direction) {
    return direction == LINKLET_DIR_RIGHT ? "right" : "left";
}

static const char *element_kind_name(const LinkletElementKind kind) {
    return kind == LINKLET_ELEMENT_NODE ? "node" : "edge";
}

static void dump_filter(const char *tag, const LinkletObjectFilter *filter, FILE *out) {
    if (!filter->label && !filter->has_id && linklet_bson_empty(&filter->properties)) {
        return;
    }
    fprintf(out, "    %s:", tag);
    if (filter->has_id) {
        fprintf(out, " id=%llu", (unsigned long long)filter->id);
    }
    if (filter->label) {
        fprintf(out, " label=\"%s\"", filter->label);
    }
    if (!linklet_bson_empty(&filter->properties)) {
        fprintf(out, " properties=%zu bytes", linklet_bson_get_length(&filter->properties));
    }
    fputc('\n', out);
}

void linklet_logical_plan_dump(const LinkletLogicalPlan *plan, FILE *out) {
    if (!out) {
        out = stderr;
    }
    if (!plan) {
        fputs("(null logical plan)\n", out);
        return;
    }
    fprintf(out, "logical_plan call_count=%zu\n", plan->call_count);
    for (size_t index = 0; index < plan->call_count; ++index) {
        const LinkletKernelCall *call = &plan->calls[index];
        fprintf(out, "  [%zu] %s", index, kernel_code_name(call->code));
        switch (call->code) {
        case LINKLET_KERNEL_MATCH:
        case LINKLET_KERNEL_UPDATE:
        case LINKLET_KERNEL_DELETE:
            fprintf(out, " direction=%s result=%s binding=%u%s", direction_name(call->direction),
                    element_kind_name(call->match.result_kind), call->match.result_binding,
                    call->match.is_edge_pattern ? " edge_pattern" : "");
            if (call->code == LINKLET_KERNEL_DELETE && call->detach) {
                fputs(" detach", out);
            }
            fputc('\n', out);
            dump_filter("source", &call->match.source, out);
            if (call->match.is_edge_pattern) {
                dump_filter("edge", &call->match.edge, out);
                dump_filter("destination", &call->match.destination, out);
            }
            break;
        case LINKLET_KERNEL_REACHABILITY:
            fprintf(out, " %llu -{%zu}-> %llu direction=%s\n", (unsigned long long)call->source_id,
                    call->max_hops, (unsigned long long)call->destination_id,
                    direction_name(call->direction));
            break;
        case LINKLET_KERNEL_INSERT:
            fprintf(out, " kind=%s", element_kind_name(call->insert_kind));
            if (call->insert_kind == LINKLET_ELEMENT_EDGE) {
                fprintf(out, " %llu -> %llu", (unsigned long long)call->insert_source_id,
                        (unsigned long long)call->insert_destination_id);
            }
            fputc('\n', out);
            break;
        default:
            fputc('\n', out);
            break;
        }
    }
}
