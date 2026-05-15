#ifndef ECSVM_INTERNAL_H
#define ECSVM_INTERNAL_H

#include "bin_internal.h"
#include "ecsvm/ecsvm.h"

#include <stddef.h>
#include <stdint.h>

typedef enum ecsvm_managed_value_kind {
    ECSVM_MANAGED_VALUE_VOID = 0,
    ECSVM_MANAGED_VALUE_NULL,
    ECSVM_MANAGED_VALUE_BOOL,
    ECSVM_MANAGED_VALUE_NUMBER,
    ECSVM_MANAGED_VALUE_STRING
} ecsvm_managed_value_kind_t;

typedef struct ecsvm_managed_value {
    ecsvm_managed_value_kind_t kind;
    int boolean_value;
    double number_value;
    uint32_t blob_id;
} ecsvm_managed_value_t;

typedef ecsvm_status_t (*ecsvm_native_function_fn)(
    ecsvm_engine_t *engine,
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_function_ref_t *function_ref,
    const ecsvm_managed_value_t *arguments,
    size_t argument_count,
    ecsvm_managed_value_t *out_value
);

typedef struct ecsvm_function_entry {
    const ecsvm_ecsbin_function_ref_t *function_ref;
    ecsvm_ecsbin_ast_blob_t managed_body;
    ecsvm_native_function_fn native_callback;
    int has_managed_body;
} ecsvm_function_entry_t;

ecsvm_native_function_fn ecsvm_core_find_native_function(const char *qualified_name);

ecsvm_status_t ecsvm_engine_load_functions(
    ecsvm_engine_t *engine,
    const ecsvm_ecsbin_module_t *module,
    char *error_message,
    size_t error_message_capacity
);

const ecsvm_function_entry_t *ecsvm_engine_function(
    const ecsvm_engine_t *engine,
    uint32_t function_id
);

void ecsvm_engine_clear_functions(ecsvm_engine_t *engine);

#endif
