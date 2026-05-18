#include "ecsvm_internal.h"

#include "bin_internal.h"
#include "ecsvm/system_time.h"

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

enum {
    ECSVM_MANAGED_STACK_BUFFER_CAPACITY = 128,
    ECSVM_MANAGED_CALL_ARGUMENT_LIMIT = 16
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
    ecsvm_managed_value_t value
)
{
    size_t index;

    for (index = 0u; index < frame->local_count; ++index) {
        if (frame->locals[index].name_blob_id == name_blob_id) {
            frame->locals[index].value = value;
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
    frame->local_count += 1u;
    return 1;
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

static int ecsvm_managed_component_pointer(
    ecsvm_managed_frame_t *frame,
    uint32_t type_id,
    ecsvm_entity_t entity,
    int create_if_missing,
    void **out_pointer
)
{
    const ecsvm_ecsbin_type_ref_t *type_ref;
    const ecsvm_ecsbin_struct_def_t *definition;
    ecsvm_component_id_t component_id;
    void *component_data;

    if (frame == NULL || out_pointer == NULL) {
        return 0;
    }

    type_ref = ecsvm_ecsbin_type_ref(frame->runtime->module, type_id);
    definition = type_ref != NULL
        ? ecsvm_ecsbin_find_struct(frame->runtime->module, type_ref->qualified_name)
        : NULL;
    if (type_ref == NULL ||
        type_ref->qualified_name == NULL ||
        definition == NULL ||
        !ecsvm_ecsbin_struct_is_component(frame->runtime->module, definition)) {
        return 0;
    }

    component_id = ecsvm_engine_find_component(frame->runtime->engine, type_ref->qualified_name);
    if (component_id == ECSVM_INVALID_COMPONENT) {
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
            ecsvm_managed_value_t value;
            ecsvm_managed_value_t reference_value;

            left_node = &frame->ast.nodes[node->first_child];
            if (ecsvm_managed_eval_expression(frame, left_node->next_sibling, &value) != ECSVM_OK) {
                return ECSVM_ERROR_ARGUMENT;
            }
            if (left_node->kind == ECSVM_ECSBIN_AST_NODE_IDENTIFIER &&
                left_node->value_kind == ECSVM_ECSBIN_AST_VALUE_BLOB_ID) {
                if (!ecsvm_managed_frame_set_local(frame, left_node->value, value)) {
                    return ECSVM_ERROR_ARGUMENT;
                }
            } else if (!ecsvm_managed_resolve_reference(frame, node->first_child, 1, &reference_value) ||
                       reference_value.kind != ECSVM_MANAGED_VALUE_REFERENCE ||
                       !ecsvm_managed_store_scalar(
                           frame->runtime->module,
                           reference_value.type_id,
                           reference_value.reference_value,
                           &value
                       )) {
                return ECSVM_ERROR_ARGUMENT;
            }
            *out_value = value;
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

            name_node = &frame->ast.nodes[node->first_child];
            value = ecsvm_managed_null_value();
            value_node = &frame->ast.nodes[name_node->next_sibling];
            if (value_node->kind == ECSVM_ECSBIN_AST_NODE_TYPE_EXPRESSION) {
                value_node = value_node->next_sibling == 0u ? NULL : &frame->ast.nodes[value_node->next_sibling];
            }
            if (value_node != NULL) {
                ecsvm_status_t status;
                status = ecsvm_managed_eval_expression(
                    frame,
                    value_node - frame->ast.nodes,
                    &value
                );
                if (status != ECSVM_OK) {
                    return status;
                }
            }
            return ecsvm_managed_frame_set_local(frame, name_node->value, value)
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
    size_t system_count;
    size_t function_index;
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
    system_count = 0u;
    exit_code = 1;

    for (function_index = 0u; function_index < module->function_ref_count; ++function_index) {
        const ecsvm_ecsbin_function_ref_t *function_ref;
        function_ref = &module->function_refs[function_index];
        if (function_ref->body_blob_id != 0u &&
            ecsvm_ecsbin_function_has_attribute(module, function_ref, "core.System")) {
            system_count += 1u;
        }
    }

    if (system_count == 0u) {
        fprintf(stderr, "no managed systems found in module\n");
        return 1;
    }

    engine = ecsvm_engine_create();
    if (engine == NULL) {
        fprintf(stderr, "failed to create engine\n");
        return 1;
    }

    runtime.engine = engine;
    runtime.module = module;
    error_message[0] = '\0';
    status = ecsvm_engine_register_builtin_components(engine);
    if (status == ECSVM_OK) {
        status = ecsvm_ecsbin_register_components(engine, module);
    }
    if (status == ECSVM_OK) {
        status = ecsvm_engine_load_functions(engine, module, error_message, sizeof(error_message));
    }
    if (status != ECSVM_OK) {
        if (error_message[0] != '\0') {
            fprintf(stderr, "failed to register managed module state: %s\n", error_message);
        } else {
            fprintf(stderr, "failed to register managed module state: %s\n", ecsvm_status_string(status));
        }
        ecsvm_engine_destroy(engine);
        return 1;
    }

    {
        ecsvm_component_id_t time_component;

        time_component = ecsvm_engine_find_component(engine, "core.Time");
        if (time_component != ECSVM_INVALID_COMPONENT) {
            ecsvm_time_system_init(&time_system, time_component);
            status = ecsvm_time_system_register(engine, &time_system);
            if (status != ECSVM_OK) {
                fprintf(stderr, "failed to register native time system: %s\n", ecsvm_status_string(status));
                ecsvm_engine_destroy(engine);
                return 1;
            }
        }
    }

    bindings = (ecsvm_managed_system_binding_t *)calloc(system_count, sizeof(*bindings));
    if (bindings == NULL) {
        fprintf(stderr, "out of memory while preparing managed systems\n");
        ecsvm_engine_destroy(engine);
        return 1;
    }

    system_count = 0u;
    for (function_index = 0u; function_index < module->function_ref_count; ++function_index) {
        const ecsvm_ecsbin_function_ref_t *function_ref;
        ecsvm_system_desc_t desc;
        const char **before_names;
        const char **after_names;
        size_t before_count;
        size_t after_count;

        function_ref = &module->function_refs[function_index];
        if (function_ref->body_blob_id == 0u ||
            !ecsvm_ecsbin_function_has_attribute(module, function_ref, "core.System")) {
            continue;
        }
        if (function_ref->parameter_count != 0u) {
            fprintf(stderr, "managed runtime currently supports only zero-parameter systems (%s)\n", function_ref->qualified_name);
            goto cleanup;
        }

        memset(&desc, 0, sizeof(desc));
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
            fprintf(stderr, "out of memory while preparing managed system dependencies\n");
            free(before_names);
            free(after_names);
            goto cleanup;
        }

        bindings[system_count].runtime = &runtime;
        bindings[system_count].function_id = (uint32_t)function_index + 1u;
        desc.name = function_ref->qualified_name;
        desc.callback = ecsvm_managed_system_callback;
        desc.user_data = &bindings[system_count];
        desc.before = before_names;
        desc.before_count = before_count;
        desc.after = after_names;
        desc.after_count = after_count;
        status = ecsvm_engine_register_system(engine, &desc, NULL);
        free(before_names);
        free(after_names);
        if (status != ECSVM_OK) {
            fprintf(stderr, "failed to register managed system %s: %s\n", function_ref->qualified_name, ecsvm_status_string(status));
            goto cleanup;
        }
        system_count += 1u;
    }

#if ECSVM_ENABLE_SDL3
    if (ecsvm_module_references_system_dependency(module, "core.Window") ||
        ecsvm_module_references_system_dependency(module, "core.Renderer")) {
        ecsvm_window_config_t window_config;

        memset(&window_config, 0, sizeof(window_config));
        window_config.title = "ecsvm";
        window_config.width = 960;
        window_config.height = 540;
        window_system = ecsvm_window_system_create(&window_config);
        if (window_system == NULL) {
            fprintf(stderr, "failed to create SDL window system\n");
            goto cleanup;
        }

        status = ecsvm_window_system_register(engine, window_system);
        if (status != ECSVM_OK) {
            fprintf(stderr, "failed to register SDL window system: %s\n", ecsvm_status_string(status));
            goto cleanup;
        }
    }

    if (ecsvm_module_references_system_dependency(module, "core.Renderer")) {
        ecsvm_renderer_config_t renderer_config;

        memset(&renderer_config, 0, sizeof(renderer_config));
        renderer_config.components.hierarchy = ecsvm_engine_hierarchy_component(engine);
        renderer_config.components.transform = ecsvm_engine_find_component(engine, "core.Transform");
        renderer_config.components.time = ecsvm_engine_find_component(engine, "core.Time");
        renderer_config.components.graphics_shape = ecsvm_engine_find_component(engine, "core.graphics.GraphicsShape");
        renderer_config.clear_color.x = 0.05f;
        renderer_config.clear_color.y = 0.05f;
        renderer_config.clear_color.z = 0.08f;
        renderer_config.clear_color.w = 1.0f;
        if (window_system == NULL ||
            renderer_config.components.transform == ECSVM_INVALID_COMPONENT ||
            renderer_config.components.graphics_shape == ECSVM_INVALID_COMPONENT) {
            fprintf(stderr, "failed to prepare SDL renderer system\n");
            goto cleanup;
        }

        renderer_system = ecsvm_renderer_system_create(window_system, &renderer_config);
        if (renderer_system == NULL) {
            fprintf(stderr, "failed to create SDL renderer system\n");
            goto cleanup;
        }

        status = ecsvm_renderer_system_register(engine, renderer_system);
        if (status != ECSVM_OK) {
            fprintf(stderr, "failed to register SDL renderer system: %s\n", ecsvm_status_string(status));
            goto cleanup;
        }
    }
#endif

    status = ecsvm_engine_run(engine);
    if (status != ECSVM_OK) {
        fprintf(stderr, "managed runtime failed: %s\n", ecsvm_status_string(status));
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
        fprintf(stderr, "failed to load ecsbin: %s\n", error_message[0] != '\0' ? error_message : ecsvm_status_string(status));
        return 1;
    }

    exit_code = ecsvm_run_loaded_ecs_module(&module);
    ecsvm_ecsbin_unload(&module);
    return exit_code;
}
