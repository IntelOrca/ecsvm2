#include "project_internal.h"

#include <stdlib.h>

#define ECSVM_DEFINE_ARRAY_PUSH(function_name, owner_type, items_field, capacity_field, count_field, value_type) \
int function_name(owner_type *owner, value_type value) \
{ \
    if (!ecsvm_reserve_bytes( \
            (void **)&owner->items_field, \
            sizeof(*owner->items_field), \
            &owner->capacity_field, \
            owner->count_field + 1u \
        )) { \
        return 0; \
    } \
 \
    owner->items_field[owner->count_field] = value; \
    owner->count_field += 1u; \
    return 1; \
}

#define ECSVM_DEFINE_ARRAY_PUSH_WITH_INDEX(function_name, owner_type, items_field, capacity_field, count_field, value_type) \
int function_name(owner_type *owner, value_type value, size_t *out_index) \
{ \
    if (!ecsvm_reserve_bytes( \
            (void **)&owner->items_field, \
            sizeof(*owner->items_field), \
            &owner->capacity_field, \
            owner->count_field + 1u \
        )) { \
        return 0; \
    } \
 \
    owner->items_field[owner->count_field] = value; \
    if (out_index != NULL) { \
        *out_index = owner->count_field; \
    } \
    owner->count_field += 1u; \
    return 1; \
}

#define ECSVM_DEFINE_PLAIN_ARRAY_FREE(function_name, array_type) \
void function_name(array_type *array) \
{ \
    free(array->items); \
    memset(array, 0, sizeof(*array)); \
}

#define ECSVM_DEFINE_ARRAY_FREE(function_name, array_type, cleanup_stmt) \
void function_name(array_type *array) \
{ \
    size_t index; \
 \
    for (index = 0u; index < array->count; ++index) { \
        cleanup_stmt; \
    } \
    free(array->items); \
    memset(array, 0, sizeof(*array)); \
}

ECSVM_DEFINE_ARRAY_PUSH(ecsvm_string_array_push, ecsvm_string_array_t, items, capacity, count, char *)
ECSVM_DEFINE_ARRAY_PUSH(ecsvm_token_array_push, ecsvm_token_array_t, items, capacity, count, ecsvm_token_t)
ECSVM_DEFINE_ARRAY_PUSH_WITH_INDEX(ecsvm_syntax_node_array_push, ecsvm_syntax_node_array_t, items, capacity, count, ecsvm_syntax_node_t)
ECSVM_DEFINE_ARRAY_PUSH(ecsvm_source_file_array_push, ecsvm_source_file_array_t, items, capacity, count, ecsvm_source_file_t)
ECSVM_DEFINE_ARRAY_PUSH(ecsvm_semantic_field_array_push, ecsvm_semantic_struct_t, fields, field_capacity, field_count, ecsvm_semantic_field_t)
ECSVM_DEFINE_ARRAY_PUSH(ecsvm_semantic_struct_array_push, ecsvm_semantic_struct_array_t, items, capacity, count, ecsvm_semantic_struct_t)
ECSVM_DEFINE_ARRAY_PUSH(ecsvm_semantic_function_parameter_push, ecsvm_semantic_function_t, parameters, parameter_capacity, parameter_count, ecsvm_semantic_parameter_t)
ECSVM_DEFINE_ARRAY_PUSH(ecsvm_semantic_function_array_push, ecsvm_semantic_function_array_t, items, capacity, count, ecsvm_semantic_function_t)
ECSVM_DEFINE_ARRAY_PUSH(ecsvm_semantic_constant_array_push, ecsvm_semantic_constant_array_t, items, capacity, count, ecsvm_semantic_constant_t)
ECSVM_DEFINE_ARRAY_PUSH(ecsvm_blob_array_push, ecsvm_blob_array_t, items, capacity, count, ecsvm_blob_entry_t)
ECSVM_DEFINE_ARRAY_PUSH(ecsvm_type_ref_builder_array_push, ecsvm_type_ref_builder_array_t, items, capacity, count, ecsvm_type_ref_builder_t)
ECSVM_DEFINE_ARRAY_PUSH(ecsvm_field_ref_builder_array_push, ecsvm_field_ref_builder_array_t, items, capacity, count, ecsvm_field_ref_builder_t)
ECSVM_DEFINE_ARRAY_PUSH(ecsvm_field_def_builder_array_push, ecsvm_field_def_builder_array_t, items, capacity, count, ecsvm_field_def_builder_t)
ECSVM_DEFINE_ARRAY_PUSH(ecsvm_function_ref_builder_array_push, ecsvm_function_ref_builder_array_t, items, capacity, count, ecsvm_function_ref_builder_t)
ECSVM_DEFINE_ARRAY_PUSH(ecsvm_parameter_builder_array_push, ecsvm_parameter_builder_array_t, items, capacity, count, ecsvm_parameter_builder_t)
ECSVM_DEFINE_ARRAY_PUSH(ecsvm_attribute_builder_array_push, ecsvm_attribute_builder_array_t, items, capacity, count, ecsvm_attribute_builder_t)
ECSVM_DEFINE_ARRAY_PUSH(ecsvm_struct_def_builder_array_push, ecsvm_struct_def_builder_array_t, items, capacity, count, ecsvm_struct_def_builder_t)
ECSVM_DEFINE_ARRAY_PUSH_WITH_INDEX(ecsvm_ast_node_array_push, ecsvm_ast_node_array_t, items, capacity, count, ecsvm_ast_node_t)

ECSVM_DEFINE_ARRAY_FREE(ecsvm_string_array_free, ecsvm_string_array_t, free(array->items[index]))
ECSVM_DEFINE_PLAIN_ARRAY_FREE(ecsvm_field_def_builder_array_free, ecsvm_field_def_builder_array_t)
ECSVM_DEFINE_PLAIN_ARRAY_FREE(ecsvm_attribute_builder_array_free, ecsvm_attribute_builder_array_t)
ECSVM_DEFINE_PLAIN_ARRAY_FREE(ecsvm_struct_def_builder_array_free, ecsvm_struct_def_builder_array_t)

void ecsvm_syntax_node_add_child(
    ecsvm_syntax_node_array_t *nodes,
    size_t parent_index,
    size_t child_index
)
{
    ecsvm_syntax_node_t *parent;
    ecsvm_syntax_node_t *child;

    parent = &nodes->items[parent_index];
    child = &nodes->items[child_index];
    child->parent = parent_index;
    if (parent->first_child == 0u) {
        parent->first_child = child_index;
        parent->last_child = child_index;
    } else {
        nodes->items[parent->last_child].next_sibling = child_index;
        parent->last_child = child_index;
    }
}

void ecsvm_source_file_free(ecsvm_source_file_t *file)
{
    if (file == NULL) {
        return;
    }

    free(file->path);
    free(file->source);
    free(file->tokens.items);
    free(file->nodes.items);
    memset(file, 0, sizeof(*file));
}

ECSVM_DEFINE_ARRAY_FREE(ecsvm_source_file_array_free, ecsvm_source_file_array_t, ecsvm_source_file_free(&array->items[index]))

int ecsvm_semantic_attribute_push(
    ecsvm_semantic_struct_t *semantic_struct,
    char *attribute,
    char *data
)
{
    size_t previous_capacity;
    char **attribute_data;

    previous_capacity = semantic_struct->attribute_capacity;
    if (!ecsvm_reserve_bytes(
            (void **)&semantic_struct->attributes,
            sizeof(*semantic_struct->attributes),
            &semantic_struct->attribute_capacity,
            semantic_struct->attribute_count + 1u
        )) {
        return 0;
    }

    if (semantic_struct->attribute_capacity != previous_capacity) {
        attribute_data = (char **)realloc(
            semantic_struct->attribute_data,
            semantic_struct->attribute_capacity * sizeof(*semantic_struct->attribute_data)
        );
        if (attribute_data == NULL) {
            return 0;
        }
        semantic_struct->attribute_data = attribute_data;
    }

    semantic_struct->attributes[semantic_struct->attribute_count] = attribute;
    semantic_struct->attribute_data[semantic_struct->attribute_count] = data;
    semantic_struct->attribute_count += 1u;
    return 1;
}

void ecsvm_semantic_struct_free(ecsvm_semantic_struct_t *semantic_struct)
{
    size_t index;

    free(semantic_struct->namespace_name);
    free(semantic_struct->name);
    free(semantic_struct->qualified_name);
    for (index = 0u; index < semantic_struct->field_count; ++index) {
        free(semantic_struct->fields[index].name);
        free(semantic_struct->fields[index].type_name);
    }
    for (index = 0u; index < semantic_struct->attribute_count; ++index) {
        free(semantic_struct->attributes[index]);
        free(semantic_struct->attribute_data[index]);
    }
    free(semantic_struct->fields);
    free(semantic_struct->attributes);
    free(semantic_struct->attribute_data);
    memset(semantic_struct, 0, sizeof(*semantic_struct));
}

ECSVM_DEFINE_ARRAY_FREE(ecsvm_semantic_struct_array_free, ecsvm_semantic_struct_array_t, ecsvm_semantic_struct_free(&array->items[index]))

int ecsvm_semantic_function_attribute_push(
    ecsvm_semantic_function_t *function,
    char *attribute_name,
    char *attribute_data
)
{
    size_t previous_capacity;
    char **attribute_data_items;

    previous_capacity = function->attribute_capacity;
    if (!ecsvm_reserve_bytes(
            (void **)&function->attributes,
            sizeof(*function->attributes),
            &function->attribute_capacity,
            function->attribute_count + 1u
        )) {
        return 0;
    }

    if (function->attribute_capacity != previous_capacity) {
        attribute_data_items = (char **)realloc(
            function->attribute_data,
            function->attribute_capacity * sizeof(*function->attribute_data)
        );
        if (attribute_data_items == NULL) {
            return 0;
        }
        function->attribute_data = attribute_data_items;
    }

    function->attributes[function->attribute_count] = attribute_name;
    function->attribute_data[function->attribute_count] = attribute_data;
    function->attribute_count += 1u;
    return 1;
}

void ecsvm_semantic_parameter_free(ecsvm_semantic_parameter_t *parameter)
{
    size_t index;

    free(parameter->name);
    free(parameter->type_name);
    free(parameter->default_value);
    for (index = 0u; index < parameter->attribute_count; ++index) {
        free(parameter->attributes[index]);
    }
    free(parameter->attributes);
    memset(parameter, 0, sizeof(*parameter));
}

void ecsvm_semantic_function_free(ecsvm_semantic_function_t *function)
{
    size_t index;

    free(function->namespace_name);
    free(function->name);
    free(function->qualified_name);
    free(function->return_type_name);
    free(function->body_nodes.items);
    for (index = 0u; index < function->parameter_count; ++index) {
        ecsvm_semantic_parameter_free(&function->parameters[index]);
    }
    for (index = 0u; index < function->attribute_count; ++index) {
        free(function->attributes[index]);
        free(function->attribute_data[index]);
    }
    free(function->parameters);
    free(function->attributes);
    free(function->attribute_data);
    memset(function, 0, sizeof(*function));
}

ECSVM_DEFINE_ARRAY_FREE(ecsvm_semantic_function_array_free, ecsvm_semantic_function_array_t, ecsvm_semantic_function_free(&array->items[index]))

void ecsvm_semantic_constant_free(ecsvm_semantic_constant_t *constant)
{
    free(constant->namespace_name);
    free(constant->name);
    free(constant->qualified_name);
    free(constant->value_text);
    memset(constant, 0, sizeof(*constant));
}

ECSVM_DEFINE_ARRAY_FREE(ecsvm_semantic_constant_array_free, ecsvm_semantic_constant_array_t, ecsvm_semantic_constant_free(&array->items[index]))
ECSVM_DEFINE_ARRAY_FREE(ecsvm_blob_array_free, ecsvm_blob_array_t, free(array->items[index].data))
ECSVM_DEFINE_ARRAY_FREE(ecsvm_type_ref_builder_array_free, ecsvm_type_ref_builder_array_t, do { free(array->items[index].namespace_name); free(array->items[index].name); free(array->items[index].qualified_name); } while (0))
ECSVM_DEFINE_ARRAY_FREE(ecsvm_field_ref_builder_array_free, ecsvm_field_ref_builder_array_t, free(array->items[index].name))
ECSVM_DEFINE_ARRAY_FREE(ecsvm_function_ref_builder_array_free, ecsvm_function_ref_builder_array_t, do { free(array->items[index].namespace_name); free(array->items[index].name); } while (0))
ECSVM_DEFINE_ARRAY_FREE(ecsvm_parameter_builder_array_free, ecsvm_parameter_builder_array_t, free(array->items[index].name))

void ecsvm_ast_node_add_child(
    ecsvm_ast_node_array_t *nodes,
    size_t parent_index,
    size_t child_index
)
{
    ecsvm_ast_node_t *parent;

    parent = &nodes->items[parent_index];
    if (parent->first_child == 0u) {
        parent->first_child = (uint32_t)child_index;
        parent->last_child = (uint32_t)child_index;
    } else {
        nodes->items[parent->last_child].next_sibling = (uint32_t)child_index;
        parent->last_child = (uint32_t)child_index;
    }
}
