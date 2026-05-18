#include "ecsvm_internal.h"

#include <stdlib.h>
#include <string.h>

static ptrdiff_t ecsvm_pipeline_find_system(
    const ecsvm_system_entry_t *systems,
    size_t system_count,
    const char *name
)
{
    size_t index;

    if (systems == NULL || name == NULL) {
        return -1;
    }

    for (index = 0u; index < system_count; ++index) {
        if (systems[index].name != NULL && strcmp(systems[index].name, name) == 0) {
            return (ptrdiff_t)index;
        }
    }

    return -1;
}

static int ecsvm_pipeline_add_edge(
    unsigned char *edges,
    size_t *indegree,
    size_t system_count,
    size_t from_index,
    size_t to_index
)
{
    size_t edge_index;

    if (edges == NULL || indegree == NULL || from_index >= system_count || to_index >= system_count) {
        return 0;
    }

    if (from_index == to_index) {
        return 1;
    }

    edge_index = from_index * system_count + to_index;
    if (edges[edge_index] != 0u) {
        return 1;
    }

    edges[edge_index] = 1u;
    indegree[to_index] += 1u;
    return 1;
}

ecsvm_status_t ecsvm_pipeline_build(
    const ecsvm_system_entry_t *systems,
    size_t system_count,
    size_t **out_order,
    size_t *out_count
)
{
    unsigned char *edges;
    size_t *indegree;
    size_t *order;
    unsigned char *selected;
    size_t system_index;
    size_t order_count;

    if (out_order == NULL || out_count == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    *out_order = NULL;
    *out_count = 0u;
    if (system_count == 0u) {
        return ECSVM_OK;
    }
    if (systems == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }
    if (system_count > SIZE_MAX / system_count) {
        return ECSVM_ERROR_MEMORY;
    }

    edges = (unsigned char *)calloc(system_count * system_count, sizeof(*edges));
    indegree = (size_t *)calloc(system_count, sizeof(*indegree));
    order = (size_t *)malloc(system_count * sizeof(*order));
    selected = (unsigned char *)calloc(system_count, sizeof(*selected));
    if (edges == NULL || indegree == NULL || order == NULL || selected == NULL) {
        free(edges);
        free(indegree);
        free(order);
        free(selected);
        return ECSVM_ERROR_MEMORY;
    }

    for (system_index = 0u; system_index < system_count; ++system_index) {
        size_t dependency_index;

        for (dependency_index = 0u;
             dependency_index < systems[system_index].before_count;
             ++dependency_index) {
            ptrdiff_t target_index;

            target_index = ecsvm_pipeline_find_system(
                systems,
                system_count,
                systems[system_index].before[dependency_index]
            );
            if (target_index < 0) {
                continue;
            }
            if (!ecsvm_pipeline_add_edge(
                    edges,
                    indegree,
                    system_count,
                    system_index,
                    (size_t)target_index
                )) {
                free(edges);
                free(indegree);
                free(order);
                free(selected);
                return ECSVM_ERROR_ARGUMENT;
            }
        }

        for (dependency_index = 0u;
             dependency_index < systems[system_index].after_count;
             ++dependency_index) {
            ptrdiff_t target_index;

            target_index = ecsvm_pipeline_find_system(
                systems,
                system_count,
                systems[system_index].after[dependency_index]
            );
            if (target_index < 0) {
                continue;
            }
            if (!ecsvm_pipeline_add_edge(
                    edges,
                    indegree,
                    system_count,
                    (size_t)target_index,
                    system_index
                )) {
                free(edges);
                free(indegree);
                free(order);
                free(selected);
                return ECSVM_ERROR_ARGUMENT;
            }
        }
    }

    order_count = 0u;
    while (order_count < system_count) {
        int found;

        found = 0;
        for (system_index = 0u; system_index < system_count; ++system_index) {
            size_t target_index;

            if (selected[system_index] != 0u || indegree[system_index] != 0u) {
                continue;
            }

            selected[system_index] = 1u;
            order[order_count] = system_index;
            order_count += 1u;
            found = 1;

            for (target_index = 0u; target_index < system_count; ++target_index) {
                size_t edge_index;

                edge_index = system_index * system_count + target_index;
                if (edges[edge_index] == 0u) {
                    continue;
                }

                edges[edge_index] = 0u;
                indegree[target_index] -= 1u;
            }
            break;
        }

        if (!found) {
            free(edges);
            free(indegree);
            free(order);
            free(selected);
            return ECSVM_ERROR_ARGUMENT;
        }
    }

    free(edges);
    free(indegree);
    free(selected);
    *out_order = order;
    *out_count = order_count;
    return ECSVM_OK;
}
