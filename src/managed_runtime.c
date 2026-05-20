#include "ecsvm_internal.h"

#include "bin_internal.h"
#include "ecsvm/project.h"
#include "ecsvm/system_time.h"
#include "system_hotreload.h"
#include "utility.h"

#if ECSVM_ENABLE_SDL3
#include "ecsvm/component.h"
#include "ecsvm/system_renderer.h"
#include "ecsvm/system_window.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ecsvm_managed_local {
    uint32_t name_blob_id;
    ecsvm_managed_value_t value;
    int is_const;
} ecsvm_managed_local_t;

typedef struct ecsvm_managed_runtime {
    ecsvm_engine_t *engine;
    const ecsvm_ecsbin_module_t *module;
} ecsvm_managed_runtime_t;

typedef struct ecsvm_managed_frame {
    ecsvm_managed_runtime_t *runtime;
    const ecsvm_ecsbin_function_ref_t *function_ref;
    ecsvm_ecsbin_ast_blob_t ast;
    ecsvm_managed_value_t *arguments;
    ecsvm_managed_local_t *locals;
    size_t local_count;
    size_t local_capacity;
    ecsvm_managed_value_t return_value;
    int has_return;
} ecsvm_managed_frame_t;

typedef struct ecsvm_managed_system_binding {
    ecsvm_managed_runtime_t *runtime;
    uint32_t function_id;
} ecsvm_managed_system_binding_t;

typedef struct ecsvm_project_runtime {
    ecsvm_engine_t *engine;
    ecsvm_managed_runtime_t managed_runtime;
    ecsvm_ecsbin_module_t *module;
    ecsvm_managed_system_binding_t *bindings;
    size_t binding_count;
    char *project_path;
    char *core_library_path;
    char *ecsbin_path;
    ecsvm_hotreload_system_t hotreload_system;
    ecsvm_time_system_t time_system;
#if ECSVM_ENABLE_SDL3
    ecsvm_window_system_t *window_system;
    ecsvm_renderer_system_t *renderer_system;
#endif
} ecsvm_project_runtime_t;

enum {
    ECSVM_MANAGED_STACK_BUFFER_CAPACITY = 128,
    ECSVM_MANAGED_CALL_ARGUMENT_LIMIT = 16,
    ECSVM_PROJECT_OUTPUT_PATH_CAPACITY = 4096
};

static const ecsvm_ecsbin_blob_t *ecsvm_managed_blob(
    const ecsvm_ecsbin_module_t *module,
    uint32_t blob_id
)
{
    return ecsvm_ecsbin_blob_ref(module, blob_id);
}

static int ecsvm_managed_blob_equals_cstr(
    const ecsvm_ecsbin_module_t *module,
    uint32_t blob_id,
    const char *text
)
{
    const ecsvm_ecsbin_blob_t *blob;
    size_t length;

    blob = ecsvm_managed_blob(module, blob_id);
    length = strlen(text);
    return blob != NULL &&
        blob->length == length &&
        (length == 0u || memcmp(blob->data, text, length) == 0);
}

static int ecsvm_managed_blob_to_double(
    const ecsvm_ecsbin_module_t *module,
    uint32_t blob_id,
    double *out_value
)
{
    const ecsvm_ecsbin_blob_t *blob;
    char stack_buffer[ECSVM_MANAGED_STACK_BUFFER_CAPACITY];
    char *buffer;
    char *endptr;
    int ok;

    blob = ecsvm_managed_blob(module, blob_id);
    if (blob == NULL || out_value == NULL) {
        return 0;
    }

    buffer = blob->length < sizeof(stack_buffer)
        ? stack_buffer
        : (char *)malloc((size_t)blob->length + 1u);
    if (buffer == NULL) {
        return 0;
    }

    memcpy(buffer, blob->data, (size_t)blob->length);
    buffer[blob->length] = '\0';
    errno = 0;
    *out_value = strtod(buffer, &endptr);
    ok = errno == 0 && endptr != buffer && *endptr == '\0';
    if (buffer != stack_buffer) {
        free(buffer);
    }
    return ok;
}

static ecsvm_managed_value_t ecsvm_managed_reference_value(
    uint32_t type_id,
    void *reference_value
)
{
    ecsvm_managed_value_t value;

    memset(&value, 0, sizeof(value));
    value.kind = ECSVM_MANAGED_VALUE_REFERENCE;
    value.type_id = type_id;
    value.reference_value = reference_value;
    return value;
}

static const char *ecsvm_managed_type_name(
    const ecsvm_ecsbin_module_t *module,
    uint32_t type_id
)
{
    const ecsvm_ecsbin_type_ref_t *type_ref;

    type_ref = ecsvm_ecsbin_type_ref(module, type_id);
    return type_ref != NULL ? type_ref->qualified_name : NULL;
}

static int ecsvm_managed_component_type(
    const ecsvm_managed_runtime_t *runtime,
    uint32_t type_id,
    const ecsvm_ecsbin_struct_def_t **out_definition,
    ecsvm_component_id_t *out_component_id
)
{
    const ecsvm_ecsbin_type_ref_t *type_ref;
    const ecsvm_ecsbin_struct_def_t *definition;
    ecsvm_component_id_t component_id;

    if (runtime == NULL) {
        return 0;
    }

    type_ref = ecsvm_ecsbin_type_ref(runtime->module, type_id);
    definition = type_ref != NULL
        ? ecsvm_ecsbin_find_struct(runtime->module, type_ref->qualified_name)
        : NULL;
    if (type_ref == NULL ||
        type_ref->qualified_name == NULL ||
        definition == NULL ||
        !ecsvm_ecsbin_struct_is_component(runtime->module, definition)) {
        return 0;
    }

    component_id = ecsvm_engine_find_component(runtime->engine, type_ref->qualified_name);
    if (component_id == ECSVM_INVALID_COMPONENT) {
        return 0;
    }

    if (out_definition != NULL) {
        *out_definition = definition;
    }
    if (out_component_id != NULL) {
        *out_component_id = component_id;
    }
    return 1;
}

static int ecsvm_managed_is_scalar_type_name(const char *qualified_name)
{
    return qualified_name != NULL &&
        (strcmp(qualified_name, "core.Entity") == 0 ||
         strcmp(qualified_name, "core.Int32") == 0 ||
         strcmp(qualified_name, "core.UInt32") == 0 ||
         strcmp(qualified_name, "core.UInt64") == 0 ||
         strcmp(qualified_name, "core.Float32") == 0 ||
         strcmp(qualified_name, "core.Bool") == 0 ||
         strcmp(qualified_name, "core.Blob") == 0 ||
         strcmp(qualified_name, "core.String") == 0);
}

static int ecsvm_managed_load_scalar(
    const ecsvm_ecsbin_module_t *module,
    uint32_t type_id,
    const void *data,
    ecsvm_managed_value_t *out_value
)
{
    const char *qualified_name;

    if (module == NULL || data == NULL || out_value == NULL) {
        return 0;
    }

    qualified_name = ecsvm_managed_type_name(module, type_id);
    if (qualified_name == NULL) {
        return 0;
    }

    memset(out_value, 0, sizeof(*out_value));
    if (strcmp(qualified_name, "core.Bool") == 0) {
        out_value->kind = ECSVM_MANAGED_VALUE_BOOL;
        out_value->boolean_value = *(const unsigned char *)data != 0;
        return 1;
    }

    out_value->kind = ECSVM_MANAGED_VALUE_NUMBER;
    if (strcmp(qualified_name, "core.Int32") == 0) {
        out_value->number_value = (double)*(const int32_t *)data;
    } else if (strcmp(qualified_name, "core.UInt32") == 0 ||
               strcmp(qualified_name, "core.Entity") == 0 ||
               strcmp(qualified_name, "core.Blob") == 0 ||
               strcmp(qualified_name, "core.String") == 0) {
        out_value->number_value = (double)*(const uint32_t *)data;
    } else if (strcmp(qualified_name, "core.UInt64") == 0) {
        out_value->number_value = (double)*(const uint64_t *)data;
    } else if (strcmp(qualified_name, "core.Float32") == 0) {
        out_value->number_value = (double)*(const float *)data;
    } else {
        return 0;
    }

    return 1;
}

static ecsvm_managed_value_t ecsvm_managed_null_value(void)
{
    ecsvm_managed_value_t value;
    memset(&value, 0, sizeof(value));
    value.kind = ECSVM_MANAGED_VALUE_NULL;
    return value;
}

static ecsvm_managed_value_t ecsvm_managed_void_value(void)
{
    ecsvm_managed_value_t value;
    memset(&value, 0, sizeof(value));
    value.kind = ECSVM_MANAGED_VALUE_VOID;
    return value;
}

static int ecsvm_managed_store_scalar(
    const ecsvm_ecsbin_module_t *module,
    uint32_t type_id,
    void *data,
    const ecsvm_managed_value_t *value
)
{
    const char *qualified_name;
    ecsvm_managed_value_t scalar;

    if (module == NULL || data == NULL || value == NULL) {
        return 0;
    }

    qualified_name = ecsvm_managed_type_name(module, type_id);
    if (qualified_name == NULL) {
        return 0;
    }

    scalar = *value;
    if (scalar.kind == ECSVM_MANAGED_VALUE_VOID ||
        scalar.kind == ECSVM_MANAGED_VALUE_NULL ||
        scalar.kind == ECSVM_MANAGED_VALUE_REFERENCE) {
        return 0;
    }

    if (strcmp(qualified_name, "core.Bool") == 0) {
        if (scalar.kind == ECSVM_MANAGED_VALUE_BOOL) {
            *(unsigned char *)data = (unsigned char)(scalar.boolean_value != 0);
            return 1;
        }
        if (scalar.kind == ECSVM_MANAGED_VALUE_NUMBER) {
            *(unsigned char *)data = (unsigned char)(scalar.number_value != 0.0);
            return 1;
        }
        return 0;
    }

    if (scalar.kind == ECSVM_MANAGED_VALUE_BOOL) {
        scalar.kind = ECSVM_MANAGED_VALUE_NUMBER;
        scalar.number_value = scalar.boolean_value ? 1.0 : 0.0;
    }
    if (scalar.kind != ECSVM_MANAGED_VALUE_NUMBER) {
        return 0;
    }

    if (strcmp(qualified_name, "core.Int32") == 0) {
        *(int32_t *)data = (int32_t)scalar.number_value;
    } else if (strcmp(qualified_name, "core.UInt32") == 0 ||
               strcmp(qualified_name, "core.Entity") == 0 ||
               strcmp(qualified_name, "core.Blob") == 0 ||
               strcmp(qualified_name, "core.String") == 0) {
        *(uint32_t *)data = (uint32_t)scalar.number_value;
    } else if (strcmp(qualified_name, "core.UInt64") == 0) {
        *(uint64_t *)data = (uint64_t)scalar.number_value;
    } else if (strcmp(qualified_name, "core.Float32") == 0) {
        *(float *)data = (float)scalar.number_value;
    } else {
        return 0;
    }

    return 1;
}

static int ecsvm_managed_frame_set_local(
    ecsvm_managed_frame_t *frame,
    uint32_t name_blob_id,
    ecsvm_managed_value_t value,
    int is_const
)
{
    size_t index;

    for (index = 0u; index < frame->local_count; ++index) {
        if (frame->locals[index].name_blob_id == name_blob_id) {
            frame->locals[index].value = value;
            frame->locals[index].is_const = is_const;
            return 1;
        }
    }

    if (frame->local_count == frame->local_capacity) {
        size_t capacity;
        ecsvm_managed_local_t *locals;

        capacity = frame->local_capacity == 0u ? 8u : frame->local_capacity * 2u;
        locals = (ecsvm_managed_local_t *)realloc(frame->locals, capacity * sizeof(*locals));
        if (locals == NULL) {
            return 0;
        }
        frame->locals = locals;
        frame->local_capacity = capacity;
    }

    frame->locals[frame->local_count].name_blob_id = name_blob_id;
    frame->locals[frame->local_count].value = value;
    frame->locals[frame->local_count].is_const = is_const;
    frame->local_count += 1u;
    return 1;
}

static int ecsvm_managed_frame_assign_local(
    ecsvm_managed_frame_t *frame,
    uint32_t name_blob_id,
    ecsvm_managed_value_t value
)
{
    size_t index;

    for (index = 0u; index < frame->local_count; ++index) {
        if (frame->locals[index].name_blob_id == name_blob_id) {
            if (frame->locals[index].is_const) {
                return 0;
            }
            frame->locals[index].value = value;
            return 1;
        }
    }

    return ecsvm_managed_frame_set_local(frame, name_blob_id, value, 0);
}

static int ecsvm_managed_frame_get_local(
    const ecsvm_managed_frame_t *frame,
    uint32_t name_blob_id,
    ecsvm_managed_value_t *out_value
)
{
    size_t index;

    for (index = 0u; index < frame->local_count; ++index) {
        if (frame->locals[index].name_blob_id == name_blob_id) {
            *out_value = frame->locals[index].value;
            return 1;
        }
    }

    return 0;
}

static ecsvm_status_t ecsvm_managed_eval_expression(
    ecsvm_managed_frame_t *frame,
    uint32_t node_index,
    ecsvm_managed_value_t *out_value
);

static int ecsvm_managed_materialize_value(
    const ecsvm_managed_runtime_t *runtime,
    const ecsvm_managed_value_t *value,
    ecsvm_managed_value_t *out_value
)
{
    const char *qualified_name;

    if (runtime == NULL || value == NULL || out_value == NULL) {
        return 0;
    }

    if (value->kind != ECSVM_MANAGED_VALUE_REFERENCE) {
        *out_value = *value;
        return 1;
    }

    if (value->reference_value == NULL) {
        return 0;
    }

    qualified_name = ecsvm_managed_type_name(runtime->module, value->type_id);
    if (!ecsvm_managed_is_scalar_type_name(qualified_name)) {
        *out_value = *value;
        return 1;
    }

    return ecsvm_managed_load_scalar(
        runtime->module,
        value->type_id,
        value->reference_value,
        out_value
    );
}

static int ecsvm_managed_is_truthy(
    const ecsvm_managed_runtime_t *runtime,
    const ecsvm_managed_value_t *value
)
{
    ecsvm_managed_value_t resolved;

    if (!ecsvm_managed_materialize_value(runtime, value, &resolved)) {
        return 0;
    }

    switch (resolved.kind) {
        case ECSVM_MANAGED_VALUE_NULL:
        case ECSVM_MANAGED_VALUE_VOID:
            return 0;
        case ECSVM_MANAGED_VALUE_BOOL:
            return resolved.boolean_value != 0;
        case ECSVM_MANAGED_VALUE_NUMBER:
            return resolved.number_value != 0.0;
        case ECSVM_MANAGED_VALUE_STRING:
            return resolved.blob_id != 0u;
        case ECSVM_MANAGED_VALUE_REFERENCE:
            return resolved.reference_value != NULL;
        default:
            return 0;
    }
}

static int ecsvm_managed_values_equal(
    const ecsvm_managed_runtime_t *runtime,
    const ecsvm_managed_value_t *left,
    const ecsvm_managed_value_t *right
)
{
    ecsvm_managed_value_t resolved_left;
    ecsvm_managed_value_t resolved_right;

    if (!ecsvm_managed_materialize_value(runtime, left, &resolved_left) ||
        !ecsvm_managed_materialize_value(runtime, right, &resolved_right)) {
        return 0;
    }

    if (resolved_left.kind != resolved_right.kind) {
        return 0;
    }

    switch (resolved_left.kind) {
        case ECSVM_MANAGED_VALUE_VOID:
        case ECSVM_MANAGED_VALUE_NULL:
            return 1;
        case ECSVM_MANAGED_VALUE_BOOL:
            return resolved_left.boolean_value == resolved_right.boolean_value;
        case ECSVM_MANAGED_VALUE_NUMBER:
            return resolved_left.number_value == resolved_right.number_value;
        case ECSVM_MANAGED_VALUE_STRING: {
            const ecsvm_ecsbin_blob_t *left_blob;
            const ecsvm_ecsbin_blob_t *right_blob;

            left_blob = ecsvm_managed_blob(runtime->module, resolved_left.blob_id);
            right_blob = ecsvm_managed_blob(runtime->module, resolved_right.blob_id);
            return left_blob != NULL &&
                right_blob != NULL &&
                left_blob->length == right_blob->length &&
                (left_blob->length == 0u || memcmp(left_blob->data, right_blob->data, (size_t)left_blob->length) == 0);
        }
        case ECSVM_MANAGED_VALUE_REFERENCE:
            return resolved_left.type_id == resolved_right.type_id &&
                resolved_left.reference_value == resolved_right.reference_value;
        default:
            return 0;
    }
}

static int ecsvm_managed_entity_from_value(
    const ecsvm_managed_runtime_t *runtime,
    const ecsvm_managed_value_t *value,
    ecsvm_entity_t *out_entity
)
{
    ecsvm_managed_value_t resolved;

    if (out_entity == NULL ||
        !ecsvm_managed_materialize_value(runtime, value, &resolved) ||
        resolved.kind != ECSVM_MANAGED_VALUE_NUMBER) {
        return 0;
    }

    *out_entity = (ecsvm_entity_t)resolved.number_value;
    return 1;
}

static int ecsvm_managed_type_size(
    const ecsvm_ecsbin_module_t *module,
    uint32_t type_id,
    size_t *out_size
)
{
    const char *qualified_name;
    size_t size;
    int struct_index;

    if (module == NULL || out_size == NULL) {
        return 0;
    }

    qualified_name = ecsvm_managed_type_name(module, type_id);
    if (qualified_name == NULL) {
        return 0;
    }

    size = ecsvm_ecsbin_builtin_layout(qualified_name, NULL);
    if (size == 0u) {
        struct_index = ecsvm_ecsbin_find_struct_index_by_type(module, type_id);
        if (struct_index < 0) {
            return 0;
        }
        size = module->struct_defs[struct_index].size;
    }
    if (size == 0u) {
        return 0;
    }

    *out_size = size;
    return 1;
}

static int ecsvm_managed_zero_reference_value(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_managed_value_t *reference
)
{
    size_t size;

    if (module == NULL ||
        reference == NULL ||
        reference->kind != ECSVM_MANAGED_VALUE_REFERENCE ||
        reference->reference_value == NULL ||
        !ecsvm_managed_type_size(module, reference->type_id, &size)) {
        return 0;
    }

    memset(reference->reference_value, 0, size);
    return 1;
}

static int ecsvm_managed_type_field(
    const ecsvm_ecsbin_module_t *module,
    uint32_t type_id,
    const char *field_name,
    size_t field_name_length,
    uint32_t *out_field_type_id,
    size_t *out_field_offset
)
{
    int struct_index;
    const ecsvm_ecsbin_struct_def_t *definition;
    size_t offset;
    size_t field_index;

    if (module == NULL ||
        field_name == NULL ||
        out_field_type_id == NULL ||
        out_field_offset == NULL) {
        return 0;
    }

    struct_index = ecsvm_ecsbin_find_struct_index_by_type(module, type_id);
    if (struct_index < 0) {
        return 0;
    }

    definition = &module->struct_defs[struct_index];
    offset = 0u;
    for (field_index = 0u; field_index < definition->field_count; ++field_index) {
        const ecsvm_ecsbin_field_ref_t *field_ref;
        const ecsvm_ecsbin_type_ref_t *field_type;
        size_t field_size;
        size_t field_alignment;
        int nested_index;

        if (definition->field_start == 0u ||
            definition->field_start - 1u + field_index >= module->field_ref_count) {
            return 0;
        }

        field_ref = &module->field_refs[definition->field_start - 1u + field_index];
        field_type = ecsvm_ecsbin_type_ref(module, field_ref->type_id);
        if (field_type == NULL || field_type->qualified_name == NULL) {
            return 0;
        }

        field_size = ecsvm_ecsbin_builtin_layout(field_type->qualified_name, &field_alignment);
        if (field_size == 0u) {
            nested_index = ecsvm_ecsbin_find_struct_index_by_type(module, field_ref->type_id);
            if (nested_index < 0) {
                return 0;
            }
            field_size = module->struct_defs[nested_index].size;
            field_alignment = module->struct_defs[nested_index].alignment;
        }

        offset = ecsvm_ecsbin_align_up(offset, field_alignment);
        if (strlen(field_ref->name) == field_name_length &&
            memcmp(field_ref->name, field_name, field_name_length) == 0) {
            *out_field_type_id = field_ref->type_id;
            *out_field_offset = offset;
            return 1;
        }
        offset += field_size;
    }

    return 0;
}

static int ecsvm_managed_apply_value_to_reference(
    ecsvm_managed_frame_t *frame,
    const ecsvm_managed_value_t *reference_value,
    uint32_t value_node_index
);

static int ecsvm_managed_apply_object_literal(
    ecsvm_managed_frame_t *frame,
    uint32_t node_index,
    const ecsvm_managed_value_t *reference_value
)
{
    const ecsvm_ecsbin_ast_node_t *node;
    uint32_t child_index;

    if (frame == NULL ||
        reference_value == NULL ||
        reference_value->kind != ECSVM_MANAGED_VALUE_REFERENCE ||
        reference_value->reference_value == NULL ||
        node_index >= frame->ast.node_count) {
        return 0;
    }

    node = &frame->ast.nodes[node_index];
    if (node->kind != ECSVM_ECSBIN_AST_NODE_OBJECT_LITERAL ||
        !ecsvm_managed_zero_reference_value(frame->runtime->module, reference_value)) {
        return 0;
    }

    child_index = node->first_child;
    while (child_index != 0u) {
        const ecsvm_ecsbin_ast_node_t *field_node;
        const ecsvm_ecsbin_blob_t *field_blob;
        ecsvm_managed_value_t field_reference;
        uint32_t field_type_id;
        size_t field_offset;

        field_node = &frame->ast.nodes[child_index];
        if (field_node->kind != ECSVM_ECSBIN_AST_NODE_OBJECT_FIELD ||
            field_node->value_kind != ECSVM_ECSBIN_AST_VALUE_BLOB_ID ||
            field_node->first_child == 0u) {
            return 0;
        }

        field_blob = ecsvm_managed_blob(frame->runtime->module, field_node->value);
        if (field_blob == NULL ||
            !ecsvm_managed_type_field(
                frame->runtime->module,
                reference_value->type_id,
                (const char *)field_blob->data,
                (size_t)field_blob->length,
                &field_type_id,
                &field_offset
            )) {
            return 0;
        }

        field_reference = ecsvm_managed_reference_value(
            field_type_id,
            (unsigned char *)reference_value->reference_value + field_offset
        );
        if (!ecsvm_managed_apply_value_to_reference(frame, &field_reference, field_node->first_child)) {
            return 0;
        }

        child_index = field_node->next_sibling;
    }

    return 1;
}

static int ecsvm_managed_apply_value_to_reference(
    ecsvm_managed_frame_t *frame,
    const ecsvm_managed_value_t *reference_value,
    uint32_t value_node_index
)
{
    const ecsvm_ecsbin_ast_node_t *value_node;
    ecsvm_managed_value_t value;

    if (frame == NULL ||
        reference_value == NULL ||
        reference_value->kind != ECSVM_MANAGED_VALUE_REFERENCE ||
        reference_value->reference_value == NULL ||
        value_node_index >= frame->ast.node_count) {
        return 0;
    }

    value_node = &frame->ast.nodes[value_node_index];
    if (value_node->kind == ECSVM_ECSBIN_AST_NODE_OBJECT_LITERAL) {
        return ecsvm_managed_apply_object_literal(frame, value_node_index, reference_value);
    }

    if (ecsvm_managed_eval_expression(frame, value_node_index, &value) != ECSVM_OK) {
        return 0;
    }

    return ecsvm_managed_store_scalar(
        frame->runtime->module,
        reference_value->type_id,
        reference_value->reference_value,
        &value
    );
}

static int ecsvm_managed_component_pointer(
    ecsvm_managed_frame_t *frame,
    uint32_t type_id,
    ecsvm_entity_t entity,
    int create_if_missing,
    void **out_pointer
)
{
    const ecsvm_ecsbin_struct_def_t *definition;
    ecsvm_component_id_t component_id;
    void *component_data;

    if (frame == NULL || out_pointer == NULL) {
        return 0;
    }

    if (!ecsvm_managed_component_type(
            frame->runtime,
            type_id,
            &definition,
            &component_id
        )) {
        return 0;
    }

    component_data = ecsvm_component_get_mutable(
        frame->runtime->engine,
        component_id,
        entity
    );
    if (component_data == NULL && create_if_missing) {
        unsigned char *initial_value;
        ecsvm_status_t status;

        initial_value = (unsigned char *)calloc(1u, definition->size);
        if (initial_value == NULL) {
            return 0;
        }

        status = ecsvm_component_set(
            frame->runtime->engine,
            component_id,
            entity,
            initial_value
        );
        free(initial_value);
        if (status != ECSVM_OK) {
            return 0;
        }

        component_data = ecsvm_component_get_mutable(
            frame->runtime->engine,
            component_id,
            entity
        );
    }

    if (component_data == NULL) {
        return 0;
    }

    *out_pointer = component_data;
    return 1;
}

static int ecsvm_managed_resolve_reference(
    ecsvm_managed_frame_t *frame,
    uint32_t node_index,
    int create_if_missing,
    ecsvm_managed_value_t *out_value
)
{
    const ecsvm_ecsbin_ast_node_t *node;

    if (frame == NULL || out_value == NULL || node_index >= frame->ast.node_count) {
        return 0;
    }

    node = &frame->ast.nodes[node_index];
    switch (node->kind) {
        case ECSVM_ECSBIN_AST_NODE_IDENTIFIER:
            if (node->value_kind == ECSVM_ECSBIN_AST_VALUE_PARAMETER_ID) {
                uint32_t parameter_index;

                parameter_index = node->value - frame->function_ref->parameter_start;
                if (frame->function_ref->parameter_start == 0u ||
                    node->value < frame->function_ref->parameter_start ||
                    parameter_index >= frame->function_ref->parameter_count) {
                    return 0;
                }
                *out_value = frame->arguments[parameter_index];
                return out_value->kind == ECSVM_MANAGED_VALUE_REFERENCE;
            }
            if (node->value_kind == ECSVM_ECSBIN_AST_VALUE_BLOB_ID &&
                ecsvm_managed_frame_get_local(frame, node->value, out_value)) {
                return out_value->kind == ECSVM_MANAGED_VALUE_REFERENCE;
            }
            return 0;
        case ECSVM_ECSBIN_AST_NODE_INDEX_EXPRESSION: {
            ecsvm_managed_value_t entity_value;
            ecsvm_entity_t entity;
            void *component_data;

            if (node->first_child == 0u ||
                node->value_kind != ECSVM_ECSBIN_AST_VALUE_TYPE_REF_ID ||
                ecsvm_managed_eval_expression(frame, node->first_child, &entity_value) != ECSVM_OK ||
                !ecsvm_managed_entity_from_value(frame->runtime, &entity_value, &entity) ||
                !ecsvm_managed_component_pointer(
                    frame,
                    node->value,
                    entity,
                    create_if_missing,
                    &component_data
                )) {
                return 0;
            }

            *out_value = ecsvm_managed_reference_value(node->value, component_data);
            return 1;
        }
        case ECSVM_ECSBIN_AST_NODE_MEMBER_EXPRESSION: {
            const ecsvm_ecsbin_ast_node_t *left_node;
            const ecsvm_ecsbin_ast_node_t *field_node;
            const ecsvm_ecsbin_blob_t *field_blob;
            ecsvm_managed_value_t base_value;
            uint32_t field_type_id;
            size_t field_offset;

            left_node = node->first_child == 0u ? NULL : &frame->ast.nodes[node->first_child];
            field_node = left_node != NULL && left_node->next_sibling != 0u
                ? &frame->ast.nodes[left_node->next_sibling]
                : NULL;
            if (field_node == NULL ||
                field_node->kind != ECSVM_ECSBIN_AST_NODE_IDENTIFIER ||
                field_node->value_kind != ECSVM_ECSBIN_AST_VALUE_BLOB_ID ||
                !ecsvm_managed_resolve_reference(frame, node->first_child, create_if_missing, &base_value) ||
                base_value.kind != ECSVM_MANAGED_VALUE_REFERENCE) {
                return 0;
            }

            field_blob = ecsvm_managed_blob(frame->runtime->module, field_node->value);
            if (field_blob == NULL ||
                !ecsvm_managed_type_field(
                    frame->runtime->module,
                    base_value.type_id,
                    (const char *)field_blob->data,
                    (size_t)field_blob->length,
                    &field_type_id,
                    &field_offset
                )) {
                return 0;
            }

            *out_value = ecsvm_managed_reference_value(
                field_type_id,
                (unsigned char *)base_value.reference_value + field_offset
            );
            return 1;
        }
        default:
            return 0;
    }
}

static ecsvm_status_t ecsvm_managed_invoke_function(
    ecsvm_managed_runtime_t *runtime,
    uint32_t function_id,
    uint32_t type_argument_id,
    const ecsvm_managed_value_t *arguments,
    size_t argument_count,
    ecsvm_managed_value_t *out_value
);

static ecsvm_status_t ecsvm_managed_eval_expression(
    ecsvm_managed_frame_t *frame,
    uint32_t node_index,
    ecsvm_managed_value_t *out_value
)
{
    const ecsvm_ecsbin_ast_node_t *node;
    const ecsvm_ecsbin_blob_t *blob;

    node = &frame->ast.nodes[node_index];
    switch (node->kind) {
        case ECSVM_ECSBIN_AST_NODE_IDENTIFIER:
            if (node->value_kind == ECSVM_ECSBIN_AST_VALUE_PARAMETER_ID) {
                uint32_t parameter_index;
                parameter_index = node->value - frame->function_ref->parameter_start;
                if (frame->function_ref->parameter_start == 0u ||
                    node->value < frame->function_ref->parameter_start ||
                    parameter_index >= frame->function_ref->parameter_count) {
                    return ECSVM_ERROR_ARGUMENT;
                }
                *out_value = frame->arguments[parameter_index];
                return ECSVM_OK;
            }
            if (node->value_kind == ECSVM_ECSBIN_AST_VALUE_BLOB_ID &&
                ecsvm_managed_frame_get_local(frame, node->value, out_value)) {
                return ECSVM_OK;
            }
            return ECSVM_ERROR_NOT_FOUND;
        case ECSVM_ECSBIN_AST_NODE_LITERAL_EXPRESSION:
            if (node->value_kind != ECSVM_ECSBIN_AST_VALUE_BLOB_ID) {
                return ECSVM_ERROR_ARGUMENT;
            }
            if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "true")) {
                out_value->kind = ECSVM_MANAGED_VALUE_BOOL;
                out_value->boolean_value = 1;
                return ECSVM_OK;
            }
            if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "false")) {
                out_value->kind = ECSVM_MANAGED_VALUE_BOOL;
                out_value->boolean_value = 0;
                return ECSVM_OK;
            }
            if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "null")) {
                *out_value = ecsvm_managed_null_value();
                return ECSVM_OK;
            }
            blob = ecsvm_managed_blob(frame->runtime->module, node->value);
            if (blob == NULL) {
                return ECSVM_ERROR_NOT_FOUND;
            }
            if (blob->length >= 2u &&
                blob->data[0] == '"' &&
                blob->data[blob->length - 1u] == '"') {
                out_value->kind = ECSVM_MANAGED_VALUE_STRING;
                out_value->blob_id = node->value;
                return ECSVM_OK;
            }
            out_value->kind = ECSVM_MANAGED_VALUE_NUMBER;
            if (!ecsvm_managed_blob_to_double(frame->runtime->module, node->value, &out_value->number_value)) {
                return ECSVM_ERROR_ARGUMENT;
            }
            return ECSVM_OK;
        case ECSVM_ECSBIN_AST_NODE_GROUPING_EXPRESSION:
            return ecsvm_managed_eval_expression(frame, node->first_child, out_value);
        case ECSVM_ECSBIN_AST_NODE_INDEX_EXPRESSION:
        case ECSVM_ECSBIN_AST_NODE_MEMBER_EXPRESSION: {
            ecsvm_managed_value_t reference_value;
            const char *qualified_name;

            if (!ecsvm_managed_resolve_reference(frame, node_index, 0, &reference_value)) {
                return ECSVM_ERROR_NOT_FOUND;
            }

            qualified_name = ecsvm_managed_type_name(frame->runtime->module, reference_value.type_id);
            if (ecsvm_managed_is_scalar_type_name(qualified_name) &&
                !ecsvm_managed_materialize_value(frame->runtime, &reference_value, out_value)) {
                return ECSVM_ERROR_ARGUMENT;
            }
            if (!ecsvm_managed_is_scalar_type_name(qualified_name)) {
                *out_value = reference_value;
            }
            return ECSVM_OK;
        }
        case ECSVM_ECSBIN_AST_NODE_UNARY_EXPRESSION: {
            ecsvm_managed_value_t operand;
            ecsvm_managed_value_t resolved_operand;
            if (ecsvm_managed_eval_expression(frame, node->first_child, &operand) != ECSVM_OK) {
                return ECSVM_ERROR_ARGUMENT;
            }
            if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "!")) {
                out_value->kind = ECSVM_MANAGED_VALUE_BOOL;
                out_value->boolean_value = !ecsvm_managed_is_truthy(frame->runtime, &operand);
                return ECSVM_OK;
            }
            if (!ecsvm_managed_materialize_value(frame->runtime, &operand, &resolved_operand)) {
                return ECSVM_ERROR_ARGUMENT;
            }
            if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "-")) {
                if (resolved_operand.kind != ECSVM_MANAGED_VALUE_NUMBER) {
                    return ECSVM_ERROR_ARGUMENT;
                }
                out_value->kind = ECSVM_MANAGED_VALUE_NUMBER;
                out_value->number_value = -resolved_operand.number_value;
                return ECSVM_OK;
            }
            if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "+")) {
                *out_value = resolved_operand;
                return ECSVM_OK;
            }
            return ECSVM_ERROR_ARGUMENT;
        }
        case ECSVM_ECSBIN_AST_NODE_BINARY_EXPRESSION: {
            const ecsvm_ecsbin_ast_node_t *right_node;
            ecsvm_managed_value_t left;
            ecsvm_managed_value_t right;
            ecsvm_managed_value_t resolved_left;
            ecsvm_managed_value_t resolved_right;

            right_node = &frame->ast.nodes[frame->ast.nodes[node->first_child].next_sibling];
            if (ecsvm_managed_eval_expression(frame, node->first_child, &left) != ECSVM_OK ||
                ecsvm_managed_eval_expression(frame, frame->ast.nodes[node->first_child].next_sibling, &right) != ECSVM_OK) {
                return ECSVM_ERROR_ARGUMENT;
            }
            if (!ecsvm_managed_materialize_value(frame->runtime, &left, &resolved_left) ||
                !ecsvm_managed_materialize_value(frame->runtime, &right, &resolved_right)) {
                return ECSVM_ERROR_ARGUMENT;
            }

            (void)right_node;
            if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "+") ||
                ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "-") ||
                ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "*") ||
                ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "/")) {
                if (resolved_left.kind != ECSVM_MANAGED_VALUE_NUMBER ||
                    resolved_right.kind != ECSVM_MANAGED_VALUE_NUMBER) {
                    return ECSVM_ERROR_ARGUMENT;
                }
                out_value->kind = ECSVM_MANAGED_VALUE_NUMBER;
                if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "+")) {
                    out_value->number_value = resolved_left.number_value + resolved_right.number_value;
                } else if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "-")) {
                    out_value->number_value = resolved_left.number_value - resolved_right.number_value;
                } else if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "*")) {
                    out_value->number_value = resolved_left.number_value * resolved_right.number_value;
                } else {
                    if (resolved_right.number_value == 0.0) {
                        return ECSVM_ERROR_ARGUMENT;
                    }
                    out_value->number_value = resolved_left.number_value / resolved_right.number_value;
                }
                return ECSVM_OK;
            }

            if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "==") ||
                ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "!=")) {
                out_value->kind = ECSVM_MANAGED_VALUE_BOOL;
                out_value->boolean_value = ecsvm_managed_values_equal(frame->runtime, &left, &right);
                if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "!=")) {
                    out_value->boolean_value = !out_value->boolean_value;
                }
                return ECSVM_OK;
            }

            if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "&&")) {
                out_value->kind = ECSVM_MANAGED_VALUE_BOOL;
                out_value->boolean_value = ecsvm_managed_is_truthy(frame->runtime, &left) &&
                    ecsvm_managed_is_truthy(frame->runtime, &right);
                return ECSVM_OK;
            }

            if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "||")) {
                out_value->kind = ECSVM_MANAGED_VALUE_BOOL;
                out_value->boolean_value = ecsvm_managed_is_truthy(frame->runtime, &left) ||
                    ecsvm_managed_is_truthy(frame->runtime, &right);
                return ECSVM_OK;
            }

            if (resolved_left.kind != ECSVM_MANAGED_VALUE_NUMBER ||
                resolved_right.kind != ECSVM_MANAGED_VALUE_NUMBER) {
                return ECSVM_ERROR_ARGUMENT;
            }

            out_value->kind = ECSVM_MANAGED_VALUE_BOOL;
            if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "<")) {
                out_value->boolean_value = resolved_left.number_value < resolved_right.number_value;
            } else if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, ">")) {
                out_value->boolean_value = resolved_left.number_value > resolved_right.number_value;
            } else if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, "<=")) {
                out_value->boolean_value = resolved_left.number_value <= resolved_right.number_value;
            } else if (ecsvm_managed_blob_equals_cstr(frame->runtime->module, node->value, ">=")) {
                out_value->boolean_value = resolved_left.number_value >= resolved_right.number_value;
            } else {
                return ECSVM_ERROR_ARGUMENT;
            }
            return ECSVM_OK;
        }
        case ECSVM_ECSBIN_AST_NODE_ASSIGNMENT_EXPRESSION: {
            const ecsvm_ecsbin_ast_node_t *left_node;
            const ecsvm_ecsbin_ast_node_t *right_node;
            ecsvm_managed_value_t value;
            ecsvm_managed_value_t reference_value;

            value = ecsvm_managed_null_value();
            reference_value = ecsvm_managed_null_value();
            left_node = &frame->ast.nodes[node->first_child];
            right_node = left_node->next_sibling == 0u
                ? NULL
                : &frame->ast.nodes[left_node->next_sibling];
            if (right_node == NULL) {
                return ECSVM_ERROR_ARGUMENT;
            }
            if (left_node->kind == ECSVM_ECSBIN_AST_NODE_IDENTIFIER &&
                left_node->value_kind == ECSVM_ECSBIN_AST_VALUE_BLOB_ID) {
                if (right_node->kind == ECSVM_ECSBIN_AST_NODE_OBJECT_LITERAL ||
                    ecsvm_managed_eval_expression(frame, left_node->next_sibling, &value) != ECSVM_OK ||
                    !ecsvm_managed_frame_assign_local(frame, left_node->value, value)) {
                    return ECSVM_ERROR_ARGUMENT;
                }
            } else if (!ecsvm_managed_resolve_reference(frame, node->first_child, 1, &reference_value) ||
                       reference_value.kind != ECSVM_MANAGED_VALUE_REFERENCE ||
                       !ecsvm_managed_apply_value_to_reference(
                           frame,
                           &reference_value,
                           left_node->next_sibling
                       )) {
                return ECSVM_ERROR_ARGUMENT;
            }
            if (right_node->kind == ECSVM_ECSBIN_AST_NODE_OBJECT_LITERAL) {
                *out_value = reference_value;
            } else {
                *out_value = value;
            }
            return ECSVM_OK;
        }
        case ECSVM_ECSBIN_AST_NODE_CALL_EXPRESSION: {
            const ecsvm_ecsbin_ast_node_t *callee;
            const ecsvm_ecsbin_ast_node_t *type_argument;
            const ecsvm_ecsbin_ast_node_t *argument_list;
            ecsvm_managed_value_t arguments[ECSVM_MANAGED_CALL_ARGUMENT_LIMIT];
            size_t argument_count;
            uint32_t child_index;
            uint32_t function_id;
            uint32_t type_argument_id;

            callee = &frame->ast.nodes[node->first_child];
            function_id = node->value_kind == ECSVM_ECSBIN_AST_VALUE_FUNCTION_REF_ID
                ? node->value
                : 0u;
            type_argument_id = 0u;
            if (function_id == 0u) {
                return ECSVM_ERROR_ARGUMENT;
            }

            type_argument = callee->next_sibling == 0u ? NULL : &frame->ast.nodes[callee->next_sibling];
            if (type_argument != NULL &&
                type_argument->kind == ECSVM_ECSBIN_AST_NODE_TYPE_EXPRESSION &&
                type_argument->value_kind == ECSVM_ECSBIN_AST_VALUE_TYPE_REF_ID) {
                type_argument_id = type_argument->value;
                argument_list = type_argument->next_sibling == 0u
                    ? NULL
                    : &frame->ast.nodes[type_argument->next_sibling];
            } else {
                argument_list = type_argument;
            }
            if (argument_list == NULL ||
                argument_list->kind != ECSVM_ECSBIN_AST_NODE_ARGUMENT_LIST) {
                return ECSVM_ERROR_ARGUMENT;
            }

            argument_count = 0u;
            child_index = argument_list->first_child;
            while (child_index != 0u) {
                if (argument_count >= sizeof(arguments) / sizeof(arguments[0]) ||
                    ecsvm_managed_eval_expression(frame, child_index, &arguments[argument_count]) != ECSVM_OK) {
                    return ECSVM_ERROR_ARGUMENT;
                }
                argument_count += 1u;
                child_index = frame->ast.nodes[child_index].next_sibling;
            }

            return ecsvm_managed_invoke_function(
                frame->runtime,
                function_id,
                type_argument_id,
                arguments,
                argument_count,
                out_value
            );
        }
        default:
            return ECSVM_ERROR_ARGUMENT;
    }
}

static ecsvm_status_t ecsvm_managed_execute_statement(
    ecsvm_managed_frame_t *frame,
    uint32_t node_index
)
{
    const ecsvm_ecsbin_ast_node_t *node;

    node = &frame->ast.nodes[node_index];
    switch (node->kind) {
        case ECSVM_ECSBIN_AST_NODE_BLOCK: {
            uint32_t child_index;
            child_index = node->first_child;
            while (child_index != 0u && !frame->has_return) {
                ecsvm_status_t status;
                status = ecsvm_managed_execute_statement(frame, child_index);
                if (status != ECSVM_OK) {
                    return status;
                }
                child_index = frame->ast.nodes[child_index].next_sibling;
            }
            return ECSVM_OK;
        }
        case ECSVM_ECSBIN_AST_NODE_DECLARATION: {
            const ecsvm_ecsbin_ast_node_t *name_node;
            const ecsvm_ecsbin_ast_node_t *value_node;
            ecsvm_managed_value_t value;
            int is_const;

            name_node = &frame->ast.nodes[node->first_child];
            value = ecsvm_managed_null_value();
            is_const = node->token_kind == ECSVM_ECSBIN_TOKEN_KEY_CONST;
            value_node = &frame->ast.nodes[name_node->next_sibling];
            if (value_node->kind == ECSVM_ECSBIN_AST_NODE_TYPE_EXPRESSION) {
                value_node = value_node->next_sibling == 0u ? NULL : &frame->ast.nodes[value_node->next_sibling];
            }
            if (value_node != NULL) {
                ecsvm_status_t status;
                status = ecsvm_managed_eval_expression(
                    frame,
                    (uint32_t)(value_node - frame->ast.nodes),
                    &value
                );
                if (status != ECSVM_OK) {
                    return status;
                }
            }
            return ecsvm_managed_frame_set_local(frame, name_node->value, value, is_const)
                ? ECSVM_OK
                : ECSVM_ERROR_MEMORY;
        }
        case ECSVM_ECSBIN_AST_NODE_RETURN_STATEMENT:
            frame->return_value = node->first_child == 0u
                ? ecsvm_managed_void_value()
                : ecsvm_managed_null_value();
            if (node->first_child != 0u) {
                ecsvm_status_t status;
                status = ecsvm_managed_eval_expression(frame, node->first_child, &frame->return_value);
                if (status != ECSVM_OK) {
                    return status;
                }
            }
            frame->has_return = 1;
            return ECSVM_OK;
        case ECSVM_ECSBIN_AST_NODE_EXPRESSION_STATEMENT: {
            ecsvm_managed_value_t value;
            return ecsvm_managed_eval_expression(frame, node->first_child, &value);
        }
        case ECSVM_ECSBIN_AST_NODE_IF_STATEMENT: {
            const ecsvm_ecsbin_ast_node_t *condition_node;
            const ecsvm_ecsbin_ast_node_t *then_node;
            const ecsvm_ecsbin_ast_node_t *else_node;
            ecsvm_managed_value_t condition;

            condition_node = &frame->ast.nodes[node->first_child];
            then_node = &frame->ast.nodes[condition_node->next_sibling];
            else_node = then_node->next_sibling == 0u ? NULL : &frame->ast.nodes[then_node->next_sibling];
            if (ecsvm_managed_eval_expression(frame, node->first_child, &condition) != ECSVM_OK) {
                return ECSVM_ERROR_ARGUMENT;
            }
            if (ecsvm_managed_is_truthy(frame->runtime, &condition)) {
                return ecsvm_managed_execute_statement(frame, condition_node->next_sibling);
            }
            if (else_node != NULL && else_node->kind == ECSVM_ECSBIN_AST_NODE_ELSE_CLAUSE &&
                else_node->first_child != 0u) {
                return ecsvm_managed_execute_statement(frame, else_node->first_child);
            }
            return ECSVM_OK;
        }
        case ECSVM_ECSBIN_AST_NODE_FOR_IN_STATEMENT: {
            const ecsvm_ecsbin_ast_node_t *identifier_node;
            const ecsvm_ecsbin_ast_node_t *type_node;
            const ecsvm_ecsbin_ast_node_t *body_node;
            ecsvm_component_id_t component_id;
            size_t index;

            identifier_node = node->first_child == 0u ? NULL : &frame->ast.nodes[node->first_child];
            type_node = (identifier_node != NULL && identifier_node->next_sibling != 0u)
                ? &frame->ast.nodes[identifier_node->next_sibling]
                : NULL;
            body_node = (type_node != NULL && type_node->next_sibling != 0u)
                ? &frame->ast.nodes[type_node->next_sibling]
                : NULL;
            if (identifier_node == NULL ||
                identifier_node->kind != ECSVM_ECSBIN_AST_NODE_IDENTIFIER ||
                type_node == NULL ||
                type_node->kind != ECSVM_ECSBIN_AST_NODE_TYPE_EXPRESSION ||
                type_node->value_kind != ECSVM_ECSBIN_AST_VALUE_TYPE_REF_ID ||
                body_node == NULL ||
                !ecsvm_managed_component_type(
                    frame->runtime,
                    type_node->value,
                    NULL,
                    &component_id
                )) {
                return ECSVM_ERROR_ARGUMENT;
            }

            for (index = 0u; index < ecsvm_entity_count(frame->runtime->engine) && !frame->has_return; ++index) {
                ecsvm_entity_t entity;
                ecsvm_managed_value_t value;
                ecsvm_status_t status;

                entity = ecsvm_entity_at(frame->runtime->engine, index);
                if (entity == ECSVM_INVALID_ENTITY ||
                    !ecsvm_component_has(frame->runtime->engine, component_id, entity)) {
                    continue;
                }

                memset(&value, 0, sizeof(value));
                value.kind = ECSVM_MANAGED_VALUE_NUMBER;
                value.number_value = (double)entity;
                if (!ecsvm_managed_frame_set_local(frame, identifier_node->value, value, 0)) {
                    return ECSVM_ERROR_MEMORY;
                }

                status = ecsvm_managed_execute_statement(
                    frame,
                    identifier_node->next_sibling == 0u
                        ? 0u
                        : type_node->next_sibling
                );
                if (status != ECSVM_OK) {
                    return status;
                }
            }

            return ECSVM_OK;
        }
        default:
            return ECSVM_ERROR_ARGUMENT;
    }
}

static ecsvm_status_t ecsvm_managed_invoke_function(
    ecsvm_managed_runtime_t *runtime,
    uint32_t function_id,
    uint32_t type_argument_id,
    const ecsvm_managed_value_t *arguments,
    size_t argument_count,
    ecsvm_managed_value_t *out_value
)
{
    const ecsvm_function_entry_t *function_entry;
    const ecsvm_ecsbin_function_ref_t *function_ref;
    ecsvm_managed_frame_t frame;
    ecsvm_status_t status;

    function_entry = ecsvm_engine_function(runtime->engine, function_id);
    if (function_entry == NULL || function_entry->function_ref == NULL) {
        return ECSVM_ERROR_NOT_FOUND;
    }

    function_ref = function_entry->function_ref;
    if (function_ref->parameter_count != argument_count) {
        return ECSVM_ERROR_ARGUMENT;
    }
    if (function_entry->native_callback != NULL) {
        return function_entry->native_callback(
            runtime->engine,
            runtime->module,
            function_ref,
            type_argument_id,
            arguments,
            argument_count,
            out_value
        );
    }
    if (!function_entry->has_managed_body) {
        return ECSVM_ERROR_NOT_FOUND;
    }

    memset(&frame, 0, sizeof(frame));
    frame.runtime = runtime;
    frame.function_ref = function_ref;
    frame.arguments = (ecsvm_managed_value_t *)arguments;
    frame.ast = function_entry->managed_body;
    frame.return_value = ecsvm_managed_void_value();

    status = ecsvm_managed_execute_statement(&frame, frame.ast.nodes[0].first_child);
    if (status == ECSVM_OK) {
        *out_value = frame.has_return ? frame.return_value : ecsvm_managed_void_value();
    }

    free(frame.locals);
    return status;
}

static ecsvm_status_t ecsvm_managed_system_callback(ecsvm_context_t *ctx)
{
    ecsvm_managed_system_binding_t *binding;
    ecsvm_managed_value_t result;

    binding = (ecsvm_managed_system_binding_t *)ctx->api.userdata;
    if (binding == NULL || binding->runtime == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    return ecsvm_managed_invoke_function(
        binding->runtime,
        binding->function_id,
        0u,
        NULL,
        0u,
        &result
    );
}

static const char *ecsvm_attribute_dependency_name(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_attribute_t *attribute
)
{
    uint32_t payload_type_id;
    const ecsvm_ecsbin_type_ref_t *payload_type;

    if (!ecsvm_ecsbin_attribute_type_payload(module, attribute, &payload_type_id)) {
        return NULL;
    }

    payload_type = ecsvm_ecsbin_type_ref(module, payload_type_id);
    return payload_type != NULL ? payload_type->qualified_name : NULL;
}

static size_t ecsvm_function_dependency_count(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_function_ref_t *function_ref,
    const char *attribute_name
)
{
    size_t attribute_index;
    size_t count;

    count = 0u;
    if (module == NULL || function_ref == NULL || attribute_name == NULL) {
        return 0u;
    }

    for (attribute_index = 1u; attribute_index < function_ref->attribute_count; ++attribute_index) {
        const ecsvm_ecsbin_attribute_t *attribute;
        const ecsvm_ecsbin_type_ref_t *type_ref;
        const char *dependency_name;

        attribute = ecsvm_ecsbin_attribute_ref(module, function_ref->attribute_start + (uint32_t)attribute_index);
        type_ref = attribute != NULL ? ecsvm_ecsbin_type_ref(module, attribute->type_id) : NULL;
        dependency_name = ecsvm_attribute_dependency_name(module, attribute);
        if (type_ref == NULL ||
            type_ref->qualified_name == NULL ||
            dependency_name == NULL) {
            continue;
        }
        if (strcmp(type_ref->qualified_name, attribute_name) == 0) {
            count += 1u;
        }
    }

    return count;
}

static int ecsvm_collect_function_dependencies(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_function_ref_t *function_ref,
    const char *attribute_name,
    const char ***out_names,
    size_t *out_count
)
{
    const char **names;
    size_t attribute_index;
    size_t write_index;

    *out_names = NULL;
    *out_count = 0u;
    *out_count = ecsvm_function_dependency_count(module, function_ref, attribute_name);
    if (*out_count == 0u) {
        return 1;
    }

    names = (const char **)calloc(*out_count, sizeof(*names));
    if (names == NULL) {
        return 0;
    }

    write_index = 0u;
    for (attribute_index = 1u; attribute_index < function_ref->attribute_count; ++attribute_index) {
        const ecsvm_ecsbin_attribute_t *attribute;
        const ecsvm_ecsbin_type_ref_t *type_ref;
        const char *dependency_name;

        attribute = ecsvm_ecsbin_attribute_ref(module, function_ref->attribute_start + (uint32_t)attribute_index);
        type_ref = attribute != NULL ? ecsvm_ecsbin_type_ref(module, attribute->type_id) : NULL;
        dependency_name = ecsvm_attribute_dependency_name(module, attribute);
        if (type_ref == NULL ||
            type_ref->qualified_name == NULL ||
            dependency_name == NULL ||
            strcmp(type_ref->qualified_name, attribute_name) != 0) {
            continue;
        }

        names[write_index] = dependency_name;
        write_index += 1u;
    }

    *out_names = names;
    *out_count = write_index;
    return 1;
}

#if ECSVM_ENABLE_SDL3
static int ecsvm_module_references_system_dependency(
    const ecsvm_ecsbin_module_t *module,
    const char *system_name
)
{
    size_t function_index;

    if (module == NULL || system_name == NULL) {
        return 0;
    }

    for (function_index = 0u; function_index < module->function_ref_count; ++function_index) {
        const ecsvm_ecsbin_function_ref_t *function_ref;
        size_t attribute_index;

        function_ref = &module->function_refs[function_index];
        for (attribute_index = 1u; attribute_index < function_ref->attribute_count; ++attribute_index) {
            const ecsvm_ecsbin_attribute_t *attribute;
            const ecsvm_ecsbin_type_ref_t *type_ref;
            const char *dependency_name;

            attribute = ecsvm_ecsbin_attribute_ref(
                module,
                function_ref->attribute_start + (uint32_t)attribute_index
            );
            type_ref = attribute != NULL ? ecsvm_ecsbin_type_ref(module, attribute->type_id) : NULL;
            dependency_name = ecsvm_attribute_dependency_name(module, attribute);
            if (type_ref == NULL ||
                type_ref->qualified_name == NULL ||
                dependency_name == NULL) {
                continue;
            }
            if ((strcmp(type_ref->qualified_name, "core.Before") == 0 ||
                 strcmp(type_ref->qualified_name, "core.After") == 0) &&
                strcmp(dependency_name, system_name) == 0) {
                return 1;
            }
        }
    }

    return 0;
}
#endif

static size_t ecsvm_count_managed_systems(const ecsvm_ecsbin_module_t *module)
{
    size_t function_index;
    size_t count;

    count = 0u;
    if (module == NULL) {
        return 0u;
    }

    for (function_index = 0u; function_index < module->function_ref_count; ++function_index) {
        const ecsvm_ecsbin_function_ref_t *function_ref;

        function_ref = &module->function_refs[function_index];
        if (function_ref->body_blob_id != 0u &&
            ecsvm_ecsbin_function_has_attribute(module, function_ref, "core.System")) {
            count += 1u;
        }
    }

    return count;
}

static int ecsvm_strings_equal(const char *left, const char *right)
{
    if (left == NULL || right == NULL) {
        return left == right;
    }

    return strcmp(left, right) == 0;
}

static void ecsvm_managed_set_error(char *error_message, size_t error_message_capacity, const char *message)
{
    if (error_message == NULL || error_message_capacity == 0u) {
        return;
    }

    if (message == NULL) {
        error_message[0] = '\0';
        return;
    }

    (void)snprintf(error_message, error_message_capacity, "%s", message);
}

static void ecsvm_managed_log_line(const char *message)
{
    if (message != NULL && message[0] != '\0') {
        fprintf(stderr, "%s\n", message);
    }
}

static void ecsvm_managed_log_prefixed(const char *prefix, const char *message)
{
    fprintf(stderr, "%s: %s\n", prefix, message != NULL ? message : "");
}

static void ecsvm_managed_log_status(
    const char *prefix,
    ecsvm_status_t status,
    const char *error_message
)
{
    fprintf(
        stderr,
        "%s: %s\n",
        prefix,
        error_message != NULL && error_message[0] != '\0'
            ? error_message
            : ecsvm_status_string(status)
    );
}

static void ecsvm_managed_unload_module(ecsvm_ecsbin_module_t *module)
{
    if (module == NULL) {
        return;
    }

    ecsvm_ecsbin_unload(module);
    free(module);
}

static ecsvm_ecsbin_module_t *ecsvm_managed_load_module(
    const char *ecsbin_path,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_ecsbin_module_t *module;
    ecsvm_status_t status;

    module = (ecsvm_ecsbin_module_t *)calloc(1u, sizeof(*module));
    if (module == NULL) {
        ecsvm_managed_set_error(error_message, error_message_capacity, "out of memory while loading ecsbin");
        return NULL;
    }

    status = ecsvm_ecsbin_load(ecsbin_path, module, error_message, error_message_capacity);
    if (status != ECSVM_OK) {
        ecsvm_managed_unload_module(module);
        return NULL;
    }

    return module;
}

static int ecsvm_allocate_managed_bindings(
    const ecsvm_ecsbin_module_t *module,
    ecsvm_managed_system_binding_t **out_bindings,
    size_t *out_count,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t count;

    *out_bindings = NULL;
    *out_count = 0u;

    count = ecsvm_count_managed_systems(module);
    if (count == 0u) {
        return 1;
    }

    *out_bindings = (ecsvm_managed_system_binding_t *)calloc(count, sizeof(**out_bindings));
    if (*out_bindings == NULL) {
        ecsvm_managed_set_error(error_message, error_message_capacity, "out of memory while preparing managed systems");
        return 0;
    }

    *out_count = count;
    return 1;
}

static int ecsvm_register_managed_systems(
    ecsvm_engine_t *engine,
    ecsvm_managed_runtime_t *runtime,
    const ecsvm_ecsbin_module_t *module,
    ecsvm_managed_system_binding_t *bindings,
    size_t binding_count,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t function_index;
    size_t binding_index;

    binding_index = 0u;
    if (engine == NULL || runtime == NULL || module == NULL) {
        ecsvm_managed_set_error(error_message, error_message_capacity, "invalid managed runtime state");
        return 0;
    }

    for (function_index = 0u; function_index < module->function_ref_count; ++function_index) {
        const ecsvm_ecsbin_function_ref_t *function_ref;
        ecsvm_system_desc_t desc;
        const char **before_names;
        const char **after_names;
        size_t before_count;
        size_t after_count;
        ecsvm_status_t status;

        function_ref = &module->function_refs[function_index];
        if (function_ref->body_blob_id == 0u ||
            !ecsvm_ecsbin_function_has_attribute(module, function_ref, "core.System")) {
            continue;
        }

        if (function_ref->parameter_count != 0u) {
            (void)snprintf(
                error_message,
                error_message_capacity,
                "managed runtime currently supports only zero-parameter systems (%s)",
                function_ref->qualified_name
            );
            return 0;
        }

        if (binding_index >= binding_count) {
            ecsvm_managed_set_error(error_message, error_message_capacity, "managed system binding table is inconsistent");
            return 0;
        }

        before_names = NULL;
        after_names = NULL;
        before_count = 0u;
        after_count = 0u;
        if (!ecsvm_collect_function_dependencies(
                module,
                function_ref,
                "core.Before",
                &before_names,
                &before_count
            ) ||
            !ecsvm_collect_function_dependencies(
                module,
                function_ref,
                "core.After",
                &after_names,
                &after_count
            )) {
            free(before_names);
            free(after_names);
            ecsvm_managed_set_error(
                error_message,
                error_message_capacity,
                "out of memory while preparing managed system dependencies"
            );
            return 0;
        }

        memset(&desc, 0, sizeof(desc));
        bindings[binding_index].runtime = runtime;
        bindings[binding_index].function_id = (uint32_t)function_index + 1u;
        desc.name = function_ref->qualified_name;
        desc.callback = ecsvm_managed_system_callback;
        desc.user_data = &bindings[binding_index];
        desc.before = before_names;
        desc.before_count = before_count;
        desc.after = after_names;
        desc.after_count = after_count;
        status = ecsvm_engine_register_system(engine, &desc, NULL);
        free(before_names);
        free(after_names);
        if (status != ECSVM_OK) {
            (void)snprintf(
                error_message,
                error_message_capacity,
                "failed to register managed system %s: %s",
                function_ref->qualified_name,
                ecsvm_status_string(status)
            );
            return 0;
        }

        binding_index += 1u;
    }

    return 1;
}

static void ecsvm_unregister_system_if_present(ecsvm_engine_t *engine, const char *system_name)
{
    if (engine == NULL || system_name == NULL) {
        return;
    }

    (void)ecsvm_engine_unregister_system(engine, system_name);
}

static void ecsvm_unregister_managed_systems(
    ecsvm_engine_t *engine,
    const ecsvm_ecsbin_module_t *module
)
{
    size_t function_index;

    if (engine == NULL || module == NULL) {
        return;
    }

    for (function_index = 0u; function_index < module->function_ref_count; ++function_index) {
        const ecsvm_ecsbin_function_ref_t *function_ref;

        function_ref = &module->function_refs[function_index];
        if (function_ref->body_blob_id != 0u &&
            ecsvm_ecsbin_function_has_attribute(module, function_ref, "core.System")) {
            ecsvm_unregister_system_if_present(engine, function_ref->qualified_name);
        }
    }
}

static void ecsvm_unregister_project_runtime_systems(
    ecsvm_engine_t *engine,
    const ecsvm_ecsbin_module_t *module
)
{
    ecsvm_unregister_system_if_present(engine, "core.Renderer");
    ecsvm_unregister_system_if_present(engine, "core.Window");
    ecsvm_unregister_system_if_present(engine, "core.Time");
    ecsvm_unregister_managed_systems(engine, module);
}

static const ecsvm_ecsbin_field_def_t *ecsvm_find_field_definition(
    const ecsvm_ecsbin_module_t *module,
    uint32_t field_id
)
{
    size_t field_index;

    if (module == NULL || field_id == 0u) {
        return NULL;
    }

    for (field_index = 0u; field_index < module->field_def_count; ++field_index) {
        if (module->field_defs[field_index].field_id == field_id) {
            return &module->field_defs[field_index];
        }
    }

    return NULL;
}

static int ecsvm_attributes_match(
    const ecsvm_ecsbin_module_t *left_module,
    uint32_t left_start,
    uint32_t left_count,
    const ecsvm_ecsbin_module_t *right_module,
    uint32_t right_start,
    uint32_t right_count
)
{
    size_t attribute_index;

    if (left_count != right_count) {
        return 0;
    }

    for (attribute_index = 0u; attribute_index < (size_t)left_count; ++attribute_index) {
        const ecsvm_ecsbin_attribute_t *left_attribute;
        const ecsvm_ecsbin_attribute_t *right_attribute;
        const ecsvm_ecsbin_type_ref_t *left_type;
        const ecsvm_ecsbin_type_ref_t *right_type;
        int left_expects_type;
        int right_expects_type;

        left_attribute = ecsvm_ecsbin_attribute_ref(left_module, left_start + (uint32_t)attribute_index);
        right_attribute = ecsvm_ecsbin_attribute_ref(right_module, right_start + (uint32_t)attribute_index);
        left_type = left_attribute != NULL ? ecsvm_ecsbin_type_ref(left_module, left_attribute->type_id) : NULL;
        right_type = right_attribute != NULL ? ecsvm_ecsbin_type_ref(right_module, right_attribute->type_id) : NULL;
        if (left_attribute == NULL ||
            right_attribute == NULL ||
            left_type == NULL ||
            right_type == NULL ||
            !ecsvm_strings_equal(left_type->qualified_name, right_type->qualified_name)) {
            return 0;
        }

        left_expects_type = ecsvm_ecsbin_attribute_expects_type_payload(left_module, left_attribute);
        right_expects_type = ecsvm_ecsbin_attribute_expects_type_payload(right_module, right_attribute);
        if (left_expects_type != right_expects_type) {
            return 0;
        }

        if (left_expects_type) {
            uint32_t left_payload_type_id;
            uint32_t right_payload_type_id;
            const ecsvm_ecsbin_type_ref_t *left_payload_type;
            const ecsvm_ecsbin_type_ref_t *right_payload_type;

            if (!ecsvm_ecsbin_attribute_type_payload(left_module, left_attribute, &left_payload_type_id) ||
                !ecsvm_ecsbin_attribute_type_payload(right_module, right_attribute, &right_payload_type_id)) {
                return 0;
            }

            left_payload_type = ecsvm_ecsbin_type_ref(left_module, left_payload_type_id);
            right_payload_type = ecsvm_ecsbin_type_ref(right_module, right_payload_type_id);
            if (left_payload_type == NULL ||
                right_payload_type == NULL ||
                !ecsvm_strings_equal(left_payload_type->qualified_name, right_payload_type->qualified_name)) {
                return 0;
            }
            continue;
        }

        if (!ecsvm_strings_equal(left_attribute->data, right_attribute->data)) {
            return 0;
        }
    }

    return 1;
}

static const ecsvm_ecsbin_struct_def_t *ecsvm_find_component_definition(
    const ecsvm_ecsbin_module_t *module,
    const char *qualified_name
)
{
    size_t struct_index;

    if (module == NULL || qualified_name == NULL) {
        return NULL;
    }

    for (struct_index = 0u; struct_index < module->struct_def_count; ++struct_index) {
        const ecsvm_ecsbin_struct_def_t *definition;
        const ecsvm_ecsbin_type_ref_t *type_ref;

        definition = &module->struct_defs[struct_index];
        if (!ecsvm_ecsbin_struct_is_component(module, definition)) {
            continue;
        }

        type_ref = ecsvm_ecsbin_type_ref(module, definition->type_id);
        if (type_ref != NULL &&
            type_ref->qualified_name != NULL &&
            strcmp(type_ref->qualified_name, qualified_name) == 0) {
            return definition;
        }
    }

    return NULL;
}

static int ecsvm_component_definitions_match(
    const ecsvm_ecsbin_module_t *left_module,
    const ecsvm_ecsbin_struct_def_t *left_definition,
    const ecsvm_ecsbin_module_t *right_module,
    const ecsvm_ecsbin_struct_def_t *right_definition
)
{
    size_t field_index;

    if (left_definition == NULL || right_definition == NULL) {
        return 0;
    }

    if (left_definition->flags != right_definition->flags ||
        left_definition->size != right_definition->size ||
        left_definition->alignment != right_definition->alignment ||
        left_definition->field_count != right_definition->field_count) {
        return 0;
    }

    if (!ecsvm_attributes_match(
            left_module,
            left_definition->attribute_start,
            left_definition->attribute_count,
            right_module,
            right_definition->attribute_start,
            right_definition->attribute_count
        )) {
        return 0;
    }

    for (field_index = 0u; field_index < (size_t)left_definition->field_count; ++field_index) {
        const ecsvm_ecsbin_field_ref_t *left_field_ref;
        const ecsvm_ecsbin_field_ref_t *right_field_ref;
        const ecsvm_ecsbin_type_ref_t *left_field_type;
        const ecsvm_ecsbin_type_ref_t *right_field_type;
        const ecsvm_ecsbin_field_def_t *left_field_definition;
        const ecsvm_ecsbin_field_def_t *right_field_definition;

        if (left_definition->field_start == 0u ||
            right_definition->field_start == 0u ||
            left_definition->field_start - 1u + field_index >= left_module->field_ref_count ||
            right_definition->field_start - 1u + field_index >= right_module->field_ref_count) {
            return 0;
        }

        left_field_ref = &left_module->field_refs[left_definition->field_start - 1u + field_index];
        right_field_ref = &right_module->field_refs[right_definition->field_start - 1u + field_index];
        left_field_type = ecsvm_ecsbin_type_ref(left_module, left_field_ref->type_id);
        right_field_type = ecsvm_ecsbin_type_ref(right_module, right_field_ref->type_id);
        if (left_field_type == NULL ||
            right_field_type == NULL ||
            !ecsvm_strings_equal(left_field_ref->name, right_field_ref->name) ||
            !ecsvm_strings_equal(left_field_type->qualified_name, right_field_type->qualified_name)) {
            return 0;
        }

        left_field_definition = ecsvm_find_field_definition(left_module, left_definition->field_start + (uint32_t)field_index);
        right_field_definition = ecsvm_find_field_definition(right_module, right_definition->field_start + (uint32_t)field_index);
        if (left_field_definition == NULL || right_field_definition == NULL) {
            if (left_field_definition != right_field_definition) {
                return 0;
            }
            continue;
        }

        if (!ecsvm_attributes_match(
                left_module,
                left_field_definition->attribute_start,
                left_field_definition->attribute_count,
                right_module,
                right_field_definition->attribute_start,
                right_field_definition->attribute_count
            )) {
            return 0;
        }
    }

    return 1;
}

static int ecsvm_components_are_hotreload_compatible(
    const ecsvm_ecsbin_module_t *previous_module,
    const ecsvm_ecsbin_module_t *next_module,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t struct_index;

    if (previous_module == NULL || next_module == NULL) {
        ecsvm_managed_set_error(error_message, error_message_capacity, "invalid hotreload module state");
        return 0;
    }

    for (struct_index = 0u; struct_index < previous_module->struct_def_count; ++struct_index) {
        const ecsvm_ecsbin_struct_def_t *previous_definition;
        const ecsvm_ecsbin_struct_def_t *next_definition;
        const ecsvm_ecsbin_type_ref_t *type_ref;

        previous_definition = &previous_module->struct_defs[struct_index];
        if (!ecsvm_ecsbin_struct_is_component(previous_module, previous_definition)) {
            continue;
        }

        type_ref = ecsvm_ecsbin_type_ref(previous_module, previous_definition->type_id);
        if (type_ref == NULL || type_ref->qualified_name == NULL) {
            ecsvm_managed_set_error(error_message, error_message_capacity, "existing component metadata is invalid");
            return 0;
        }

        next_definition = ecsvm_find_component_definition(next_module, type_ref->qualified_name);
        if (next_definition == NULL) {
            (void)snprintf(
                error_message,
                error_message_capacity,
                "hotreload requires restart because component %s was removed",
                type_ref->qualified_name
            );
            return 0;
        }

        if (!ecsvm_component_definitions_match(previous_module, previous_definition, next_module, next_definition)) {
            (void)snprintf(
                error_message,
                error_message_capacity,
                "hotreload requires restart because component %s changed",
                type_ref->qualified_name
            );
            return 0;
        }
    }

    return 1;
}

static ecsvm_status_t ecsvm_register_new_components(
    ecsvm_engine_t *engine,
    const ecsvm_ecsbin_module_t *module,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t struct_index;

    ecsvm_managed_set_error(error_message, error_message_capacity, NULL);
    if (engine == NULL || module == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    for (struct_index = 0u; struct_index < module->struct_def_count; ++struct_index) {
        const ecsvm_ecsbin_struct_def_t *definition;
        const ecsvm_ecsbin_type_ref_t *type_ref;
        ecsvm_component_desc_t desc;
        ecsvm_status_t status;

        definition = &module->struct_defs[struct_index];
        if (!ecsvm_ecsbin_struct_is_component(module, definition)) {
            continue;
        }

        type_ref = ecsvm_ecsbin_type_ref(module, definition->type_id);
        if (type_ref == NULL || type_ref->qualified_name == NULL || definition->size == 0u) {
            ecsvm_managed_set_error(error_message, error_message_capacity, "component metadata is invalid");
            return ECSVM_ERROR_ARGUMENT;
        }

        if (strcmp(type_ref->qualified_name, "core.Hierarchy") == 0) {
            continue;
        }

        if (ecsvm_engine_find_component(engine, type_ref->qualified_name) != ECSVM_INVALID_COMPONENT) {
            continue;
        }

        desc.name = type_ref->qualified_name;
        desc.size = definition->size;
        desc.preferred_storage = ECSVM_STORAGE_CONTIGUOUS;
        status = ecsvm_engine_register_component(engine, &desc, NULL);
        if (status != ECSVM_OK) {
            (void)snprintf(
                error_message,
                error_message_capacity,
                "failed to register component %s: %s",
                type_ref->qualified_name,
                ecsvm_status_string(status)
            );
            return status;
        }
    }

    return ECSVM_OK;
}

#if ECSVM_ENABLE_SDL3
static int ecsvm_module_requires_renderer_system(const ecsvm_ecsbin_module_t *module)
{
    return ecsvm_module_references_system_dependency(module, "core.Renderer");
}

static int ecsvm_module_requires_window_system(const ecsvm_ecsbin_module_t *module)
{
    return ecsvm_module_requires_renderer_system(module) ||
        ecsvm_module_references_system_dependency(module, "core.Window");
}

static ecsvm_window_system_t *ecsvm_create_default_window_system(void)
{
    ecsvm_window_config_t window_config;

    memset(&window_config, 0, sizeof(window_config));
    window_config.title = "ecsvm";
    window_config.width = 960;
    window_config.height = 540;
    return ecsvm_window_system_create(&window_config);
}

static ecsvm_renderer_system_t *ecsvm_create_default_renderer_system(
    ecsvm_engine_t *engine,
    ecsvm_window_system_t *window_system
)
{
    ecsvm_renderer_config_t renderer_config;

    if (engine == NULL || window_system == NULL) {
        return NULL;
    }

    memset(&renderer_config, 0, sizeof(renderer_config));
    renderer_config.components.hierarchy = ecsvm_engine_hierarchy_component(engine);
    renderer_config.components.transform = ecsvm_engine_find_component(engine, "core.Transform");
    renderer_config.components.time = ecsvm_engine_find_component(engine, "core.Time");
    renderer_config.components.graphics_shape = ecsvm_engine_find_component(engine, "core.graphics.GraphicsShape");
    renderer_config.clear_color.x = 0.05f;
    renderer_config.clear_color.y = 0.05f;
    renderer_config.clear_color.z = 0.08f;
    renderer_config.clear_color.w = 1.0f;
    if (renderer_config.components.transform == ECSVM_INVALID_COMPONENT ||
        renderer_config.components.graphics_shape == ECSVM_INVALID_COMPONENT) {
        return NULL;
    }

    return ecsvm_renderer_system_create(window_system, &renderer_config);
}
#endif

static int ecsvm_register_native_systems(
    ecsvm_engine_t *engine,
    const ecsvm_ecsbin_module_t *module,
    ecsvm_time_system_t *time_system,
#if ECSVM_ENABLE_SDL3
    ecsvm_window_system_t *window_system,
    ecsvm_renderer_system_t *renderer_system,
#endif
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_component_id_t time_component;
    ecsvm_status_t status;

    if (engine == NULL || module == NULL || time_system == NULL) {
        ecsvm_managed_set_error(error_message, error_message_capacity, "invalid native runtime state");
        return 0;
    }

    time_component = ecsvm_engine_find_component(engine, "core.Time");
    if (time_component != ECSVM_INVALID_COMPONENT) {
        if (time_system->time_component != time_component) {
            ecsvm_time_system_init(time_system, time_component);
        }

        status = ecsvm_time_system_register(engine, time_system);
        if (status != ECSVM_OK) {
            (void)snprintf(
                error_message,
                error_message_capacity,
                "failed to register native time system: %s",
                ecsvm_status_string(status)
            );
            return 0;
        }
    }

#if ECSVM_ENABLE_SDL3
    if (ecsvm_module_requires_window_system(module)) {
        if (window_system == NULL) {
            ecsvm_managed_set_error(error_message, error_message_capacity, "failed to create SDL window system");
            return 0;
        }

        status = ecsvm_window_system_register(engine, window_system);
        if (status != ECSVM_OK) {
            (void)snprintf(
                error_message,
                error_message_capacity,
                "failed to register SDL window system: %s",
                ecsvm_status_string(status)
            );
            return 0;
        }
    }

    if (ecsvm_module_requires_renderer_system(module)) {
        if (renderer_system == NULL) {
            ecsvm_managed_set_error(error_message, error_message_capacity, "failed to create SDL renderer system");
            return 0;
        }

        status = ecsvm_renderer_system_register(engine, renderer_system);
        if (status != ECSVM_OK) {
            (void)snprintf(
                error_message,
                error_message_capacity,
                "failed to register SDL renderer system: %s",
                ecsvm_status_string(status)
            );
            return 0;
        }
    }
#else
    (void)error_message;
    (void)error_message_capacity;
#endif

    return 1;
}

static ecsvm_status_t ecsvm_prepare_engine_for_module(
    ecsvm_engine_t *engine,
    const ecsvm_ecsbin_module_t *module,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_status_t status;

    error_message[0] = '\0';
    status = ecsvm_engine_register_builtin_components(engine);
    if (status == ECSVM_OK) {
        status = ecsvm_ecsbin_register_components(engine, module);
    }
    if (status == ECSVM_OK) {
        status = ecsvm_engine_load_functions(engine, module, error_message, error_message_capacity);
    }
    return status;
}

static int ecsvm_register_module_runtime(
    ecsvm_engine_t *engine,
    ecsvm_managed_runtime_t *runtime,
    const ecsvm_ecsbin_module_t *module,
    ecsvm_time_system_t *time_system,
#if ECSVM_ENABLE_SDL3
    ecsvm_window_system_t *window_system,
    ecsvm_renderer_system_t *renderer_system,
#endif
    ecsvm_managed_system_binding_t *bindings,
    size_t binding_count,
    char *error_message,
    size_t error_message_capacity
)
{
    runtime->module = module;
    return ecsvm_register_native_systems(
               engine,
               module,
               time_system,
#if ECSVM_ENABLE_SDL3
               window_system,
               renderer_system,
#endif
               error_message,
               error_message_capacity
           ) &&
        ecsvm_register_managed_systems(
            engine,
            runtime,
            module,
            bindings,
            binding_count,
            error_message,
            error_message_capacity
        );
}

static int ecsvm_allocate_and_register_module_runtime(
    ecsvm_engine_t *engine,
    ecsvm_managed_runtime_t *runtime,
    const ecsvm_ecsbin_module_t *module,
    ecsvm_time_system_t *time_system,
#if ECSVM_ENABLE_SDL3
    ecsvm_window_system_t *window_system,
    ecsvm_renderer_system_t *renderer_system,
#endif
    ecsvm_managed_system_binding_t **bindings,
    size_t *binding_count,
    char *error_message,
    size_t error_message_capacity
)
{
    if (!ecsvm_allocate_managed_bindings(
            module,
            bindings,
            binding_count,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    if (!ecsvm_register_module_runtime(
            engine,
            runtime,
            module,
            time_system,
#if ECSVM_ENABLE_SDL3
            window_system,
            renderer_system,
#endif
            *bindings,
            *binding_count,
            error_message,
            error_message_capacity
        )) {
        free(*bindings);
        *bindings = NULL;
        *binding_count = 0u;
        return 0;
    }

    return 1;
}

#if ECSVM_ENABLE_SDL3
static int ecsvm_create_optional_sdl_systems(
    ecsvm_engine_t *engine,
    const ecsvm_ecsbin_module_t *module,
    ecsvm_window_system_t **window_system,
    ecsvm_renderer_system_t **renderer_system,
    const char *log_prefix
)
{
    if (ecsvm_module_requires_window_system(module)) {
        *window_system = ecsvm_create_default_window_system();
        if (*window_system == NULL) {
            ecsvm_managed_log_prefixed(log_prefix, "failed to create SDL window system");
            return 0;
        }
    }

    if (ecsvm_module_requires_renderer_system(module)) {
        *renderer_system = ecsvm_create_default_renderer_system(engine, *window_system);
        if (*renderer_system == NULL) {
            ecsvm_managed_log_prefixed(log_prefix, "failed to create SDL renderer system");
            if (*window_system != NULL) {
                ecsvm_window_system_destroy(*window_system);
                *window_system = NULL;
            }
            return 0;
        }
    }

    return 1;
}
#endif

static int ecsvm_restore_previous_project_runtime(ecsvm_project_runtime_t *runtime)
{
    char error_message[512];
    ecsvm_status_t status;

    if (runtime == NULL || runtime->engine == NULL || runtime->module == NULL) {
        return 0;
    }

    ecsvm_unregister_project_runtime_systems(runtime->engine, runtime->module);
    status = ecsvm_engine_load_functions(
        runtime->engine,
        runtime->module,
        error_message,
        sizeof(error_message)
    );
    if (status != ECSVM_OK) {
        ecsvm_managed_log_status("hotreload: failed to restore previous function table", status, error_message);
        return 0;
    }

    if (!ecsvm_register_module_runtime(
            runtime->engine,
            &runtime->managed_runtime,
            runtime->module,
            &runtime->time_system,
#if ECSVM_ENABLE_SDL3
            runtime->window_system,
            runtime->renderer_system,
#endif
            runtime->bindings,
            runtime->binding_count,
            error_message,
            sizeof(error_message)
        )) {
        ecsvm_managed_log_prefixed("hotreload: failed to restore previous systems", error_message);
        return 0;
    }

    return 1;
}

static int ecsvm_project_runtime_reload(ecsvm_project_runtime_t *runtime)
{
    char output_path[ECSVM_PROJECT_OUTPUT_PATH_CAPACITY];
    char error_message[512];
    ecsvm_ecsbin_module_t *next_module;
    ecsvm_managed_system_binding_t *next_bindings;
    size_t next_binding_count;
    ecsvm_status_t status;
#if ECSVM_ENABLE_SDL3
    ecsvm_window_system_t *next_window_system;
    ecsvm_renderer_system_t *next_renderer_system;
    ecsvm_window_system_t *created_window_system;
    ecsvm_renderer_system_t *created_renderer_system;
    int keep_window_system;
    int keep_renderer_system;
#endif

    if (runtime == NULL || runtime->engine == NULL || runtime->module == NULL) {
        return -1;
    }

    output_path[0] = '\0';
    error_message[0] = '\0';
    status = ecsvm_project_build_with_core(
        runtime->project_path,
        runtime->core_library_path,
        output_path,
        sizeof(output_path),
        error_message,
        sizeof(error_message)
    );
    if (status != ECSVM_OK) {
        ecsvm_managed_log_status("hotreload: build failed", status, error_message);
        return 0;
    }

    next_module = ecsvm_managed_load_module(output_path, error_message, sizeof(error_message));
    if (next_module == NULL) {
        ecsvm_managed_log_prefixed("hotreload: failed to load ecsbin", error_message);
        return 0;
    }

    if (!ecsvm_components_are_hotreload_compatible(
            runtime->module,
            next_module,
            error_message,
            sizeof(error_message)
        )) {
        ecsvm_managed_log_line(error_message);
        ecsvm_managed_unload_module(next_module);
        return 0;
    }

    next_bindings = NULL;
    next_binding_count = 0u;
    if (!ecsvm_allocate_managed_bindings(
            next_module,
            &next_bindings,
            &next_binding_count,
            error_message,
            sizeof(error_message)
        )) {
        ecsvm_managed_log_prefixed("hotreload", error_message);
        ecsvm_managed_unload_module(next_module);
        return 0;
    }

#if ECSVM_ENABLE_SDL3
    next_window_system = runtime->window_system;
    next_renderer_system = runtime->renderer_system;
    created_window_system = NULL;
    created_renderer_system = NULL;
    keep_window_system = ecsvm_module_requires_window_system(next_module);
    keep_renderer_system = ecsvm_module_requires_renderer_system(next_module);

    if (keep_window_system && next_window_system == NULL) {
        created_window_system = ecsvm_create_default_window_system();
        if (created_window_system == NULL) {
            ecsvm_managed_log_line("hotreload: failed to create SDL window system");
            free(next_bindings);
            ecsvm_managed_unload_module(next_module);
            return 0;
        }
        next_window_system = created_window_system;
    }

    if (keep_renderer_system && next_renderer_system == NULL) {
        created_renderer_system = ecsvm_create_default_renderer_system(runtime->engine, next_window_system);
        if (created_renderer_system == NULL) {
            ecsvm_managed_log_line("hotreload: failed to create SDL renderer system");
            ecsvm_window_system_destroy(created_window_system);
            free(next_bindings);
            ecsvm_managed_unload_module(next_module);
            return 0;
        }
        next_renderer_system = created_renderer_system;
    }
#endif

    status = ecsvm_register_new_components(
        runtime->engine,
        next_module,
        error_message,
        sizeof(error_message)
    );
    if (status != ECSVM_OK) {
        ecsvm_managed_log_prefixed("hotreload", error_message);
#if ECSVM_ENABLE_SDL3
        ecsvm_renderer_system_destroy(created_renderer_system);
        ecsvm_window_system_destroy(created_window_system);
#endif
        free(next_bindings);
        ecsvm_managed_unload_module(next_module);
        return 0;
    }

    ecsvm_unregister_project_runtime_systems(runtime->engine, runtime->module);
    status = ecsvm_engine_load_functions(
        runtime->engine,
        next_module,
        error_message,
        sizeof(error_message)
    );
    if (status != ECSVM_OK) {
        ecsvm_managed_log_status("hotreload: failed to reload function table", status, error_message);
        ecsvm_unregister_project_runtime_systems(runtime->engine, next_module);
        if (!ecsvm_restore_previous_project_runtime(runtime)) {
            ecsvm_managed_log_line("hotreload: previous runtime state could not be restored");
            status = ECSVM_ERROR_CALLBACK;
        }
#if ECSVM_ENABLE_SDL3
        ecsvm_renderer_system_destroy(created_renderer_system);
        ecsvm_window_system_destroy(created_window_system);
#endif
        free(next_bindings);
        ecsvm_managed_unload_module(next_module);
        return status == ECSVM_ERROR_CALLBACK ? -1 : 0;
    }

    if (!ecsvm_register_module_runtime(
            runtime->engine,
            &runtime->managed_runtime,
            next_module,
            &runtime->time_system,
#if ECSVM_ENABLE_SDL3
            next_window_system,
            next_renderer_system,
#endif
            next_bindings,
            next_binding_count,
            error_message,
            sizeof(error_message)
        )) {
        ecsvm_managed_log_prefixed("hotreload", error_message);
        ecsvm_unregister_project_runtime_systems(runtime->engine, next_module);
        if (!ecsvm_restore_previous_project_runtime(runtime)) {
            ecsvm_managed_log_line("hotreload: previous runtime state could not be restored");
#if ECSVM_ENABLE_SDL3
            ecsvm_renderer_system_destroy(created_renderer_system);
            ecsvm_window_system_destroy(created_window_system);
#endif
            free(next_bindings);
            ecsvm_managed_unload_module(next_module);
            return -1;
        }

#if ECSVM_ENABLE_SDL3
        ecsvm_renderer_system_destroy(created_renderer_system);
        ecsvm_window_system_destroy(created_window_system);
#endif
        free(next_bindings);
        ecsvm_managed_unload_module(next_module);
        return 0;
    }

#if ECSVM_ENABLE_SDL3
    if (!keep_renderer_system && runtime->renderer_system != NULL) {
        ecsvm_renderer_system_destroy(runtime->renderer_system);
        runtime->renderer_system = NULL;
    } else if (created_renderer_system != NULL) {
        runtime->renderer_system = created_renderer_system;
        created_renderer_system = NULL;
    }

    if (!keep_window_system && runtime->window_system != NULL) {
        ecsvm_window_system_destroy(runtime->window_system);
        runtime->window_system = NULL;
    } else if (created_window_system != NULL) {
        runtime->window_system = created_window_system;
        created_window_system = NULL;
    }
#endif

    ecsvm_managed_unload_module(runtime->module);
    free(runtime->bindings);
    runtime->module = next_module;
    runtime->bindings = next_bindings;
    runtime->binding_count = next_binding_count;
    runtime->managed_runtime.module = runtime->module;
    ecsvm_managed_log_line("hotreload: reloaded project systems");
    return 1;
}

static void ecsvm_project_runtime_free(ecsvm_project_runtime_t *runtime)
{
    if (runtime == NULL) {
        return;
    }

    ecsvm_hotreload_system_free(&runtime->hotreload_system);
    free(runtime->bindings);
    ecsvm_managed_unload_module(runtime->module);
#if ECSVM_ENABLE_SDL3
    ecsvm_renderer_system_destroy(runtime->renderer_system);
    ecsvm_window_system_destroy(runtime->window_system);
#endif
    ecsvm_engine_destroy(runtime->engine);
    free(runtime->project_path);
    free(runtime->core_library_path);
    free(runtime->ecsbin_path);
    memset(runtime, 0, sizeof(*runtime));
}

static int ecsvm_run_loaded_ecs_module(const ecsvm_ecsbin_module_t *module)
{
    ecsvm_engine_t *engine;
    ecsvm_managed_runtime_t runtime;
    ecsvm_managed_system_binding_t *bindings;
    ecsvm_time_system_t time_system;
#if ECSVM_ENABLE_SDL3
    ecsvm_window_system_t *window_system;
    ecsvm_renderer_system_t *renderer_system;
#endif
    size_t binding_count;
    ecsvm_status_t status;
    char error_message[256];
    int exit_code;

    memset(&runtime, 0, sizeof(runtime));
    memset(&time_system, 0, sizeof(time_system));
    bindings = NULL;
#if ECSVM_ENABLE_SDL3
    window_system = NULL;
    renderer_system = NULL;
#endif
    binding_count = 0u;
    exit_code = 1;

    if (ecsvm_count_managed_systems(module) == 0u) {
        ecsvm_managed_log_line("no managed systems found in module");
        return 1;
    }

    engine = ecsvm_engine_create();
    if (engine == NULL) {
        ecsvm_managed_log_line("failed to create engine");
        return 1;
    }

    runtime.engine = engine;
    status = ecsvm_prepare_engine_for_module(engine, module, error_message, sizeof(error_message));
    if (status != ECSVM_OK) {
        ecsvm_managed_log_status("failed to register managed module state", status, error_message);
        ecsvm_engine_destroy(engine);
        return 1;
    }

#if ECSVM_ENABLE_SDL3
    if (!ecsvm_create_optional_sdl_systems(
            engine,
            module,
            &window_system,
            &renderer_system,
            "run"
        )) {
        goto cleanup;
    }
#endif

    if (!ecsvm_allocate_and_register_module_runtime(
            engine,
            &runtime,
            module,
            &time_system,
#if ECSVM_ENABLE_SDL3
            window_system,
            renderer_system,
#endif
            &bindings,
            &binding_count,
            error_message,
            sizeof(error_message)
        )) {
        ecsvm_managed_log_line(error_message);
        goto cleanup;
    }

    status = ecsvm_engine_run(engine);
    if (status != ECSVM_OK) {
        ecsvm_managed_log_status("managed runtime failed", status, NULL);
        goto cleanup;
    }

    exit_code = 0;

cleanup:
    free(bindings);
#if ECSVM_ENABLE_SDL3
    ecsvm_renderer_system_destroy(renderer_system);
    ecsvm_window_system_destroy(window_system);
#endif
    ecsvm_engine_destroy(engine);
    return exit_code;
}

int ecsvm_run_ecsbin(const char *ecsbin_path)
{
    ecsvm_ecsbin_module_t module;
    char error_message[512];
    ecsvm_status_t status;
    int exit_code;

    memset(&module, 0, sizeof(module));
    status = ecsvm_ecsbin_load(
        ecsbin_path,
        &module,
        error_message,
        sizeof(error_message)
    );
    if (status != ECSVM_OK) {
        ecsvm_managed_log_status("failed to load ecsbin", status, error_message);
        return 1;
    }

    exit_code = ecsvm_run_loaded_ecs_module(&module);
    ecsvm_ecsbin_unload(&module);
    return exit_code;
}

int ecsvm_run_project(const char *project_path, const char *core_library_path, const char *ecsbin_path)
{
    ecsvm_project_runtime_t runtime;
    char error_message[512];
    ecsvm_status_t status;
    int exit_code;

    memset(&runtime, 0, sizeof(runtime));
    runtime.project_path = ecsvm_copy_string(project_path);
    runtime.core_library_path = core_library_path != NULL ? ecsvm_copy_string(core_library_path) : NULL;
    runtime.ecsbin_path = ecsvm_copy_string(ecsbin_path);
    if (runtime.project_path == NULL ||
        (core_library_path != NULL && runtime.core_library_path == NULL) ||
        runtime.ecsbin_path == NULL) {
        ecsvm_managed_log_line("failed to prepare project runtime");
        ecsvm_project_runtime_free(&runtime);
        return 1;
    }

    runtime.module = ecsvm_managed_load_module(runtime.ecsbin_path, error_message, sizeof(error_message));
    if (runtime.module == NULL) {
        ecsvm_managed_log_prefixed("failed to load ecsbin", error_message);
        ecsvm_project_runtime_free(&runtime);
        return 1;
    }

    runtime.engine = ecsvm_engine_create();
    if (runtime.engine == NULL) {
        ecsvm_managed_log_line("failed to create engine");
        ecsvm_project_runtime_free(&runtime);
        return 1;
    }

    runtime.managed_runtime.engine = runtime.engine;
    status = ecsvm_prepare_engine_for_module(
        runtime.engine,
        runtime.module,
        error_message,
        sizeof(error_message)
    );
    if (status != ECSVM_OK) {
        ecsvm_managed_log_status("failed to register managed module state", status, error_message);
        ecsvm_project_runtime_free(&runtime);
        return 1;
    }

    if (!ecsvm_hotreload_system_init(
            &runtime.hotreload_system,
            runtime.project_path,
            error_message,
            sizeof(error_message)
        )) {
        ecsvm_managed_log_prefixed("failed to initialize hotreload watcher", error_message);
        ecsvm_project_runtime_free(&runtime);
        return 1;
    }

    status = ecsvm_hotreload_system_register(runtime.engine, &runtime.hotreload_system);
    if (status != ECSVM_OK) {
        ecsvm_managed_log_status("failed to register hotreload system", status, NULL);
        ecsvm_project_runtime_free(&runtime);
        return 1;
    }

#if ECSVM_ENABLE_SDL3
    if (!ecsvm_create_optional_sdl_systems(
            runtime.engine,
            runtime.module,
            &runtime.window_system,
            &runtime.renderer_system,
            "run"
        )) {
        ecsvm_project_runtime_free(&runtime);
        return 1;
    }
#endif

    if (!ecsvm_allocate_and_register_module_runtime(
            runtime.engine,
            &runtime.managed_runtime,
            runtime.module,
            &runtime.time_system,
#if ECSVM_ENABLE_SDL3
            runtime.window_system,
            runtime.renderer_system,
#endif
            &runtime.bindings,
            &runtime.binding_count,
            error_message,
            sizeof(error_message)
        )) {
        ecsvm_managed_log_line(error_message);
        ecsvm_project_runtime_free(&runtime);
        return 1;
    }

    exit_code = 1;
    status = ECSVM_OK;
    ecsvm_engine_clear_stop(runtime.engine);
    while (!ecsvm_engine_stop_requested(runtime.engine)) {
        status = ecsvm_engine_tick(runtime.engine);
        if (status != ECSVM_OK) {
            ecsvm_managed_log_status("managed runtime failed", status, NULL);
            break;
        }

        if (ecsvm_hotreload_system_consume_reload(&runtime.hotreload_system)) {
            int reload_status;

            reload_status = ecsvm_project_runtime_reload(&runtime);
            if (reload_status < 0) {
                status = ECSVM_ERROR_CALLBACK;
                break;
            }
        }
    }

    if (!ecsvm_engine_stop_requested(runtime.engine) || status == ECSVM_OK) {
        exit_code = status == ECSVM_OK ? 0 : 1;
    }

    ecsvm_project_runtime_free(&runtime);
    return exit_code;
}
