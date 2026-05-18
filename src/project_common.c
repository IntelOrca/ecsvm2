#include "project_internal.h"

#include <stdlib.h>

void ecsvm_set_error(char *error_message, size_t capacity, const char *message)
{
    if (error_message == NULL || capacity == 0u) {
        return;
    }

    if (message == NULL) {
        error_message[0] = '\0';
        return;
    }

    (void)snprintf(error_message, capacity, "%s", message);
}

char *ecsvm_copy_string(const char *text)
{
    char *copy;
    size_t length;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1u);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1u);
    return copy;
}

char *ecsvm_copy_string_range(const char *text, size_t length)
{
    char *copy;

    copy = (char *)malloc(length + 1u);
    if (copy == NULL) {
        return NULL;
    }

    if (length > 0u) {
        memcpy(copy, text, length);
    }
    copy[length] = '\0';
    return copy;
}

size_t ecsvm_next_capacity(size_t current, size_t minimum)
{
    size_t capacity;

    capacity = current == 0u ? 4u : current;
    while (capacity < minimum) {
        capacity *= 2u;
    }

    return capacity;
}

int ecsvm_reserve_bytes(void **items, size_t item_size, size_t *capacity, size_t minimum)
{
    void *memory;
    size_t new_capacity;

    if (minimum <= *capacity) {
        return 1;
    }

    new_capacity = ecsvm_next_capacity(*capacity, minimum);
    memory = realloc(*items, item_size * new_capacity);
    if (memory == NULL) {
        return 0;
    }

    *items = memory;
    *capacity = new_capacity;
    return 1;
}

int ecsvm_string_array_push(ecsvm_string_array_t *array, char *value)
{
    if (!ecsvm_reserve_bytes(
            (void **)&array->items,
            sizeof(*array->items),
            &array->capacity,
            array->count + 1u
        )) {
        return 0;
    }

    array->items[array->count] = value;
    array->count += 1u;
    return 1;
}

void ecsvm_string_array_free(ecsvm_string_array_t *array)
{
    size_t index;

    for (index = 0u; index < array->count; ++index) {
        free(array->items[index]);
    }
    free(array->items);
    memset(array, 0, sizeof(*array));
}

int ecsvm_token_array_push(ecsvm_token_array_t *array, ecsvm_token_t token)
{
    if (!ecsvm_reserve_bytes(
            (void **)&array->items,
            sizeof(*array->items),
            &array->capacity,
            array->count + 1u
        )) {
        return 0;
    }

    array->items[array->count] = token;
    array->count += 1u;
    return 1;
}

int ecsvm_syntax_node_array_push(
    ecsvm_syntax_node_array_t *array,
    ecsvm_syntax_node_t node,
    size_t *out_index
)
{
    if (!ecsvm_reserve_bytes(
            (void **)&array->items,
            sizeof(*array->items),
            &array->capacity,
            array->count + 1u
        )) {
        return 0;
    }

    array->items[array->count] = node;
    if (out_index != NULL) {
        *out_index = array->count;
    }
    array->count += 1u;
    return 1;
}

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

int ecsvm_source_file_array_push(
    ecsvm_source_file_array_t *array,
    ecsvm_source_file_t file
)
{
    if (!ecsvm_reserve_bytes(
            (void **)&array->items,
            sizeof(*array->items),
            &array->capacity,
            array->count + 1u
        )) {
        return 0;
    }

    array->items[array->count] = file;
    array->count += 1u;
    return 1;
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

void ecsvm_source_file_array_free(ecsvm_source_file_array_t *array)
{
    size_t index;

    for (index = 0u; index < array->count; ++index) {
        ecsvm_source_file_free(&array->items[index]);
    }
    free(array->items);
    memset(array, 0, sizeof(*array));
}

int ecsvm_semantic_field_array_push(
    ecsvm_semantic_struct_t *semantic_struct,
    ecsvm_semantic_field_t field
)
{
    if (!ecsvm_reserve_bytes(
            (void **)&semantic_struct->fields,
            sizeof(*semantic_struct->fields),
            &semantic_struct->field_capacity,
            semantic_struct->field_count + 1u
        )) {
        return 0;
    }

    semantic_struct->fields[semantic_struct->field_count] = field;
    semantic_struct->field_count += 1u;
    return 1;
}

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

int ecsvm_semantic_struct_array_push(
    ecsvm_semantic_struct_array_t *array,
    ecsvm_semantic_struct_t semantic_struct
)
{
    if (!ecsvm_reserve_bytes(
            (void **)&array->items,
            sizeof(*array->items),
            &array->capacity,
            array->count + 1u
        )) {
        return 0;
    }

    array->items[array->count] = semantic_struct;
    array->count += 1u;
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

void ecsvm_semantic_struct_array_free(ecsvm_semantic_struct_array_t *array)
{
    size_t index;

    for (index = 0u; index < array->count; ++index) {
        ecsvm_semantic_struct_free(&array->items[index]);
    }
    free(array->items);
    memset(array, 0, sizeof(*array));
}

int ecsvm_semantic_function_parameter_push(
    ecsvm_semantic_function_t *function,
    ecsvm_semantic_parameter_t parameter
)
{
    if (!ecsvm_reserve_bytes(
            (void **)&function->parameters,
            sizeof(*function->parameters),
            &function->parameter_capacity,
            function->parameter_count + 1u
        )) {
        return 0;
    }

    function->parameters[function->parameter_count] = parameter;
    function->parameter_count += 1u;
    return 1;
}

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

int ecsvm_semantic_function_array_push(
    ecsvm_semantic_function_array_t *array,
    ecsvm_semantic_function_t function
)
{
    if (!ecsvm_reserve_bytes(
            (void **)&array->items,
            sizeof(*array->items),
            &array->capacity,
            array->count + 1u
        )) {
        return 0;
    }

    array->items[array->count] = function;
    array->count += 1u;
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

void ecsvm_semantic_function_array_free(ecsvm_semantic_function_array_t *array)
{
    size_t index;

    for (index = 0u; index < array->count; ++index) {
        ecsvm_semantic_function_free(&array->items[index]);
    }
    free(array->items);
    memset(array, 0, sizeof(*array));
}

int ecsvm_blob_array_push(ecsvm_blob_array_t *array, ecsvm_blob_entry_t entry)
{
    if (!ecsvm_reserve_bytes(
            (void **)&array->items,
            sizeof(*array->items),
            &array->capacity,
            array->count + 1u
        )) {
        return 0;
    }

    array->items[array->count] = entry;
    array->count += 1u;
    return 1;
}

void ecsvm_blob_array_free(ecsvm_blob_array_t *array)
{
    size_t index;

    for (index = 0u; index < array->count; ++index) {
        free(array->items[index].data);
    }
    free(array->items);
    memset(array, 0, sizeof(*array));
}

int ecsvm_type_ref_builder_array_push(
    ecsvm_type_ref_builder_array_t *array,
    ecsvm_type_ref_builder_t value
)
{
    if (!ecsvm_reserve_bytes(
            (void **)&array->items,
            sizeof(*array->items),
            &array->capacity,
            array->count + 1u
        )) {
        return 0;
    }

    array->items[array->count] = value;
    array->count += 1u;
    return 1;
}

void ecsvm_type_ref_builder_array_free(ecsvm_type_ref_builder_array_t *array)
{
    size_t index;

    for (index = 0u; index < array->count; ++index) {
        free(array->items[index].namespace_name);
        free(array->items[index].name);
        free(array->items[index].qualified_name);
    }
    free(array->items);
    memset(array, 0, sizeof(*array));
}

int ecsvm_field_ref_builder_array_push(
    ecsvm_field_ref_builder_array_t *array,
    ecsvm_field_ref_builder_t value
)
{
    if (!ecsvm_reserve_bytes(
            (void **)&array->items,
            sizeof(*array->items),
            &array->capacity,
            array->count + 1u
        )) {
        return 0;
    }

    array->items[array->count] = value;
    array->count += 1u;
    return 1;
}

void ecsvm_field_ref_builder_array_free(ecsvm_field_ref_builder_array_t *array)
{
    size_t index;

    for (index = 0u; index < array->count; ++index) {
        free(array->items[index].name);
    }
    free(array->items);
    memset(array, 0, sizeof(*array));
}

int ecsvm_field_def_builder_array_push(
    ecsvm_field_def_builder_array_t *array,
    ecsvm_field_def_builder_t value
)
{
    if (!ecsvm_reserve_bytes(
            (void **)&array->items,
            sizeof(*array->items),
            &array->capacity,
            array->count + 1u
        )) {
        return 0;
    }

    array->items[array->count] = value;
    array->count += 1u;
    return 1;
}

void ecsvm_field_def_builder_array_free(ecsvm_field_def_builder_array_t *array)
{
    free(array->items);
    memset(array, 0, sizeof(*array));
}

int ecsvm_function_ref_builder_array_push(
    ecsvm_function_ref_builder_array_t *array,
    ecsvm_function_ref_builder_t value
)
{
    if (!ecsvm_reserve_bytes(
            (void **)&array->items,
            sizeof(*array->items),
            &array->capacity,
            array->count + 1u
        )) {
        return 0;
    }

    array->items[array->count] = value;
    array->count += 1u;
    return 1;
}

void ecsvm_function_ref_builder_array_free(ecsvm_function_ref_builder_array_t *array)
{
    size_t index;

    for (index = 0u; index < array->count; ++index) {
        free(array->items[index].namespace_name);
        free(array->items[index].name);
    }
    free(array->items);
    memset(array, 0, sizeof(*array));
}

int ecsvm_parameter_builder_array_push(
    ecsvm_parameter_builder_array_t *array,
    ecsvm_parameter_builder_t value
)
{
    if (!ecsvm_reserve_bytes(
            (void **)&array->items,
            sizeof(*array->items),
            &array->capacity,
            array->count + 1u
        )) {
        return 0;
    }

    array->items[array->count] = value;
    array->count += 1u;
    return 1;
}

void ecsvm_parameter_builder_array_free(ecsvm_parameter_builder_array_t *array)
{
    size_t index;

    for (index = 0u; index < array->count; ++index) {
        free(array->items[index].name);
    }
    free(array->items);
    memset(array, 0, sizeof(*array));
}

int ecsvm_attribute_builder_array_push(
    ecsvm_attribute_builder_array_t *array,
    ecsvm_attribute_builder_t value
)
{
    if (!ecsvm_reserve_bytes(
            (void **)&array->items,
            sizeof(*array->items),
            &array->capacity,
            array->count + 1u
        )) {
        return 0;
    }

    array->items[array->count] = value;
    array->count += 1u;
    return 1;
}

void ecsvm_attribute_builder_array_free(ecsvm_attribute_builder_array_t *array)
{
    free(array->items);
    memset(array, 0, sizeof(*array));
}

int ecsvm_struct_def_builder_array_push(
    ecsvm_struct_def_builder_array_t *array,
    ecsvm_struct_def_builder_t value
)
{
    if (!ecsvm_reserve_bytes(
            (void **)&array->items,
            sizeof(*array->items),
            &array->capacity,
            array->count + 1u
        )) {
        return 0;
    }

    array->items[array->count] = value;
    array->count += 1u;
    return 1;
}

void ecsvm_struct_def_builder_array_free(ecsvm_struct_def_builder_array_t *array)
{
    free(array->items);
    memset(array, 0, sizeof(*array));
}

int ecsvm_ast_node_array_push(
    ecsvm_ast_node_array_t *array,
    ecsvm_ast_node_t node,
    size_t *out_index
)
{
    if (!ecsvm_reserve_bytes(
            (void **)&array->items,
            sizeof(*array->items),
            &array->capacity,
            array->count + 1u
        )) {
        return 0;
    }

    array->items[array->count] = node;
    if (out_index != NULL) {
        *out_index = array->count;
    }
    array->count += 1u;
    return 1;
}

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
