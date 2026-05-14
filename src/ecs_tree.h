#ifndef ECSVM_ECS_TREE_H
#define ECSVM_ECS_TREE_H

#include "project_internal.h"
#include "xml.h"

typedef struct ecsvm_ecs_tree {
    const ecsvm_source_file_t *source_file;
    const ecsvm_diagnostic_t *diagnostic;
} ecsvm_ecs_tree_t;

void ecsvm_ecs_tree_init(
    ecsvm_ecs_tree_t *tree,
    const ecsvm_source_file_t *source_file,
    const ecsvm_diagnostic_t *diagnostic
);
int ecsvm_ecs_tree_write_xml(
    const ecsvm_ecs_tree_t *tree,
    ecsvm_xml_writer_t *writer
);

#endif
