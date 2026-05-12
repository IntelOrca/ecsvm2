#ifndef ECSVM_ECSBIN_H
#define ECSVM_ECSBIN_H

#include "ecsvm/ecsvm.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    ECSVM_ECSBIN_STRUCT_FLAG_COMPONENT = 1u
};

typedef struct ecsvm_ecsbin_type_ref {
    char *namespace_name;
    char *name;
    char *qualified_name;
} ecsvm_ecsbin_type_ref_t;

typedef struct ecsvm_ecsbin_field_ref {
    char *name;
    uint32_t type_id;
} ecsvm_ecsbin_field_ref_t;

typedef struct ecsvm_ecsbin_field_def {
    uint32_t field_id;
    uint32_t attribute_start;
    uint32_t attribute_count;
} ecsvm_ecsbin_field_def_t;

typedef struct ecsvm_ecsbin_attribute {
    uint32_t type_id;
    char *data;
} ecsvm_ecsbin_attribute_t;

typedef struct ecsvm_ecsbin_struct_def {
    uint32_t type_id;
    uint32_t flags;
    uint32_t field_start;
    uint32_t field_count;
    uint32_t attribute_start;
    uint32_t attribute_count;
    size_t size;
    size_t alignment;
} ecsvm_ecsbin_struct_def_t;

typedef struct ecsvm_ecsbin_blob {
    uint64_t offset;
    uint64_t length;
    unsigned char *data;
} ecsvm_ecsbin_blob_t;

typedef struct ecsvm_ecsbin_module {
    ecsvm_ecsbin_type_ref_t *type_refs;
    ecsvm_ecsbin_field_ref_t *field_refs;
    ecsvm_ecsbin_struct_def_t *struct_defs;
    ecsvm_ecsbin_field_def_t *field_defs;
    ecsvm_ecsbin_attribute_t *attributes;
    ecsvm_ecsbin_blob_t *blobs;
    size_t type_ref_count;
    size_t field_ref_count;
    size_t struct_def_count;
    size_t field_def_count;
    size_t attribute_count;
    size_t blob_count;
} ecsvm_ecsbin_module_t;

ecsvm_status_t ecsvm_ecsbin_load(
    const char *path,
    ecsvm_ecsbin_module_t *out_module,
    char *error_message,
    size_t error_message_capacity
);

void ecsvm_ecsbin_unload(ecsvm_ecsbin_module_t *module);

const ecsvm_ecsbin_type_ref_t *ecsvm_ecsbin_type_ref(
    const ecsvm_ecsbin_module_t *module,
    uint32_t type_id
);

const ecsvm_ecsbin_struct_def_t *ecsvm_ecsbin_find_struct(
    const ecsvm_ecsbin_module_t *module,
    const char *qualified_name
);

int ecsvm_ecsbin_struct_is_component(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_struct_def_t *definition
);

ecsvm_status_t ecsvm_ecsbin_register_components(
    ecsvm_engine_t *engine,
    const ecsvm_ecsbin_module_t *module
);

#ifdef __cplusplus
}
#endif

#endif
