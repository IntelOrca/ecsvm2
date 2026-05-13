#include "ecsvm/project.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#define ECSVM_PATH_SEPARATOR '\\'
#define ecsvm_stricmp _stricmp
#else
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define ECSVM_PATH_SEPARATOR '/'
#define ecsvm_stricmp strcasecmp
#endif

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ECSVM_ALIGNOF(type) offsetof(struct { char pad; type value; }, value)

typedef struct ecsvm_manifest {
    char *name;
    char *version;
    char *entry;
} ecsvm_manifest_t;

typedef struct ecsvm_string_array {
    char **items;
    size_t count;
    size_t capacity;
} ecsvm_string_array_t;

typedef enum ecsvm_token_kind {
    ECSVM_TOKEN_EOF = 0,
    ECSVM_TOKEN_IDENTIFIER,
    ECSVM_TOKEN_NUMBER,
    ECSVM_TOKEN_STRING,
    ECSVM_TOKEN_LBRACE,
    ECSVM_TOKEN_RBRACE,
    ECSVM_TOKEN_LBRACKET,
    ECSVM_TOKEN_RBRACKET,
    ECSVM_TOKEN_LPAREN,
    ECSVM_TOKEN_RPAREN,
    ECSVM_TOKEN_COLON,
    ECSVM_TOKEN_SEMICOLON,
    ECSVM_TOKEN_DOT,
    ECSVM_TOKEN_COMMA,
    ECSVM_TOKEN_EQUAL,
    ECSVM_TOKEN_BANG,
    ECSVM_TOKEN_PLUS,
    ECSVM_TOKEN_MINUS,
    ECSVM_TOKEN_STAR,
    ECSVM_TOKEN_SLASH,
    ECSVM_TOKEN_PERCENT,
    ECSVM_TOKEN_LT,
    ECSVM_TOKEN_GT,
    ECSVM_TOKEN_AMPERSAND,
    ECSVM_TOKEN_PIPE,
    ECSVM_TOKEN_CARET,
    ECSVM_TOKEN_TILDE,
    ECSVM_TOKEN_KEY_IMPORT,
    ECSVM_TOKEN_KEY_NAMESPACE,
    ECSVM_TOKEN_KEY_STRUCT,
    ECSVM_TOKEN_KEY_COMPONENT,
    ECSVM_TOKEN_KEY_SYSTEM,
    ECSVM_TOKEN_KEY_CONST,
    ECSVM_TOKEN_KEY_FN
} ecsvm_token_kind_t;

typedef struct ecsvm_token {
    ecsvm_token_kind_t kind;
    size_t offset;
    size_t length;
    size_t line;
    size_t column;
} ecsvm_token_t;

typedef struct ecsvm_token_array {
    ecsvm_token_t *items;
    size_t count;
    size_t capacity;
} ecsvm_token_array_t;

typedef enum ecsvm_syntax_kind {
    ECSVM_SYNTAX_ROOT = 0,
    ECSVM_SYNTAX_FILE,
    ECSVM_SYNTAX_IMPORT,
    ECSVM_SYNTAX_NAMESPACE,
    ECSVM_SYNTAX_STRUCT,
    ECSVM_SYNTAX_FIELD,
    ECSVM_SYNTAX_ATTRIBUTE,
    ECSVM_SYNTAX_FUNCTION,
    ECSVM_SYNTAX_PARAMETER
} ecsvm_syntax_kind_t;

typedef struct ecsvm_syntax_node {
    ecsvm_syntax_kind_t kind;
    size_t parent;
    size_t first_child;
    size_t last_child;
    size_t next_sibling;
    size_t token_start;
    size_t token_end;
    size_t name_start;
    size_t name_end;
    size_t type_start;
    size_t type_end;
    size_t value_start;
    size_t value_end;
    size_t body_start;
    size_t body_end;
    int is_component;
    int has_body;
} ecsvm_syntax_node_t;

typedef struct ecsvm_syntax_node_array {
    ecsvm_syntax_node_t *items;
    size_t count;
    size_t capacity;
} ecsvm_syntax_node_array_t;

typedef struct ecsvm_source_file {
    char *path;
    char *source;
    size_t length;
    ecsvm_token_array_t tokens;
    ecsvm_syntax_node_array_t nodes;
} ecsvm_source_file_t;

typedef struct ecsvm_source_file_array {
    ecsvm_source_file_t *items;
    size_t count;
    size_t capacity;
} ecsvm_source_file_array_t;

typedef struct ecsvm_parser {
    const ecsvm_source_file_t *file;
    size_t index;
} ecsvm_parser_t;

typedef struct ecsvm_semantic_field {
    char *name;
    char *type_name;
} ecsvm_semantic_field_t;

typedef struct ecsvm_semantic_struct {
    char *namespace_name;
    char *name;
    char *qualified_name;
    ecsvm_semantic_field_t *fields;
    size_t field_count;
    size_t field_capacity;
    char **attributes;
    size_t attribute_count;
    size_t attribute_capacity;
    size_t size;
    size_t alignment;
    int is_component;
    int layout_state;
    int emit_state;
} ecsvm_semantic_struct_t;

typedef struct ecsvm_semantic_parameter {
    char *name;
    char *type_name;
    char **attributes;
    size_t attribute_count;
    size_t attribute_capacity;
    char *default_value;
} ecsvm_semantic_parameter_t;

typedef struct ecsvm_semantic_function {
    char *namespace_name;
    char *name;
    char *qualified_name;
    ecsvm_semantic_parameter_t *parameters;
    size_t parameter_count;
    size_t parameter_capacity;
    char **attributes;
    size_t attribute_count;
    size_t attribute_capacity;
    char *return_type_name;
    unsigned char *body_ast;
    size_t body_ast_length;
} ecsvm_semantic_function_t;

typedef struct ecsvm_semantic_struct_array {
    ecsvm_semantic_struct_t *items;
    size_t count;
    size_t capacity;
} ecsvm_semantic_struct_array_t;

typedef struct ecsvm_semantic_function_array {
    ecsvm_semantic_function_t *items;
    size_t count;
    size_t capacity;
} ecsvm_semantic_function_array_t;

typedef struct ecsvm_blob_entry {
    unsigned char *data;
    size_t length;
} ecsvm_blob_entry_t;

typedef struct ecsvm_blob_array {
    ecsvm_blob_entry_t *items;
    size_t count;
    size_t capacity;
} ecsvm_blob_array_t;

typedef struct ecsvm_type_ref_builder {
    char *namespace_name;
    char *name;
    char *qualified_name;
    uint32_t namespace_blob_id;
    uint32_t name_blob_id;
} ecsvm_type_ref_builder_t;

typedef struct ecsvm_type_ref_builder_array {
    ecsvm_type_ref_builder_t *items;
    size_t count;
    size_t capacity;
} ecsvm_type_ref_builder_array_t;

typedef struct ecsvm_field_ref_builder {
    char *name;
    uint32_t name_blob_id;
    uint32_t type_id;
} ecsvm_field_ref_builder_t;

typedef struct ecsvm_field_ref_builder_array {
    ecsvm_field_ref_builder_t *items;
    size_t count;
    size_t capacity;
} ecsvm_field_ref_builder_array_t;

typedef struct ecsvm_field_def_builder {
    uint32_t field_id;
    uint32_t attribute_start;
    uint32_t attribute_count;
} ecsvm_field_def_builder_t;

typedef struct ecsvm_field_def_builder_array {
    ecsvm_field_def_builder_t *items;
    size_t count;
    size_t capacity;
} ecsvm_field_def_builder_array_t;

typedef struct ecsvm_function_ref_builder {
    char *namespace_name;
    char *name;
    uint32_t namespace_blob_id;
    uint32_t name_blob_id;
    uint32_t parameter_start;
    uint32_t parameter_count;
    uint32_t attribute_start;
    uint32_t attribute_count;
    uint32_t body_blob_id;
} ecsvm_function_ref_builder_t;

typedef struct ecsvm_function_ref_builder_array {
    ecsvm_function_ref_builder_t *items;
    size_t count;
    size_t capacity;
} ecsvm_function_ref_builder_array_t;

typedef struct ecsvm_parameter_builder {
    char *name;
    uint32_t name_blob_id;
    uint32_t type_id;
    uint32_t attribute_start;
    uint32_t attribute_count;
    uint32_t default_value_blob_id;
} ecsvm_parameter_builder_t;

typedef struct ecsvm_parameter_builder_array {
    ecsvm_parameter_builder_t *items;
    size_t count;
    size_t capacity;
} ecsvm_parameter_builder_array_t;

typedef struct ecsvm_attribute_builder {
    uint32_t type_id;
    uint32_t data_blob_id;
} ecsvm_attribute_builder_t;

typedef struct ecsvm_attribute_builder_array {
    ecsvm_attribute_builder_t *items;
    size_t count;
    size_t capacity;
} ecsvm_attribute_builder_array_t;

typedef struct ecsvm_struct_def_builder {
    uint32_t type_id;
    uint32_t flags;
    uint32_t field_start;
    uint32_t field_count;
    uint32_t attribute_start;
    uint32_t attribute_count;
} ecsvm_struct_def_builder_t;

typedef struct ecsvm_struct_def_builder_array {
    ecsvm_struct_def_builder_t *items;
    size_t count;
    size_t capacity;
} ecsvm_struct_def_builder_array_t;

typedef struct ecsvm_ecsbin_header {
    char magic[5];
    unsigned char version[3];
    uint64_t type_reference_offset;
    uint64_t field_reference_offset;
    uint64_t struct_definition_offset;
    uint64_t field_definition_offset;
    uint64_t function_reference_offset;
    uint64_t parameter_offset;
    uint64_t attribute_offset;
    uint64_t blob_offset;
    uint32_t type_reference_count;
    uint32_t field_reference_count;
    uint32_t struct_definition_count;
    uint32_t field_definition_count;
    uint32_t function_reference_count;
    uint32_t parameter_count;
    uint32_t attribute_count;
    uint32_t blob_count;
} ecsvm_ecsbin_header_t;

typedef struct ecsvm_type_ref_disk {
    uint32_t namespace_blob_id;
    uint32_t name_blob_id;
} ecsvm_type_ref_disk_t;

typedef struct ecsvm_field_ref_disk {
    uint32_t name_blob_id;
    uint32_t type_id;
} ecsvm_field_ref_disk_t;

typedef struct ecsvm_struct_def_disk {
    uint32_t type_id;
    uint32_t flags;
    uint32_t field_start;
    uint32_t field_count;
    uint32_t attribute_start;
    uint32_t attribute_count;
} ecsvm_struct_def_disk_t;

typedef struct ecsvm_field_def_disk {
    uint32_t field_id;
    uint32_t attribute_start;
    uint32_t attribute_count;
} ecsvm_field_def_disk_t;

typedef struct ecsvm_function_ref_disk {
    uint32_t namespace_blob_id;
    uint32_t name_blob_id;
    uint32_t parameter_start;
    uint32_t parameter_count;
    uint32_t attribute_start;
    uint32_t attribute_count;
    uint32_t body_blob_id;
} ecsvm_function_ref_disk_t;

typedef struct ecsvm_parameter_disk {
    uint32_t name_blob_id;
    uint32_t type_id;
    uint32_t attribute_start;
    uint32_t attribute_count;
    uint32_t default_value_blob_id;
} ecsvm_parameter_disk_t;

typedef struct ecsvm_attribute_disk {
    uint32_t type_id;
    uint32_t data_blob_id;
} ecsvm_attribute_disk_t;

typedef struct ecsvm_blob_disk {
    uint64_t offset;
    uint64_t length;
} ecsvm_blob_disk_t;

typedef enum ecsvm_ast_node_kind {
    ECSVM_AST_NODE_ROOT = 1,
    ECSVM_AST_NODE_BLOCK,
    ECSVM_AST_NODE_GROUP_PAREN,
    ECSVM_AST_NODE_GROUP_BRACKET,
    ECSVM_AST_NODE_TOKEN
} ecsvm_ast_node_kind_t;

typedef struct ecsvm_ast_node {
    uint32_t kind;
    uint32_t first_child;
    uint32_t last_child;
    uint32_t next_sibling;
    uint32_t token_kind;
    uint32_t text_offset;
    uint32_t text_length;
} ecsvm_ast_node_t;

typedef struct ecsvm_ast_node_array {
    ecsvm_ast_node_t *items;
    size_t count;
    size_t capacity;
} ecsvm_ast_node_array_t;

enum {
    ECSVM_ECSBIN_STRUCT_FLAG_COMPONENT = 1u
};

static void ecsvm_set_error(char *error_message, size_t capacity, const char *message)
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

static char *ecsvm_copy_string(const char *text)
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

static char *ecsvm_copy_string_range(const char *text, size_t length)
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

static size_t ecsvm_next_capacity(size_t current, size_t minimum)
{
    size_t capacity;

    capacity = current == 0u ? 4u : current;
    while (capacity < minimum) {
        capacity *= 2u;
    }

    return capacity;
}

static int ecsvm_reserve_bytes(void **items, size_t item_size, size_t *capacity, size_t minimum)
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

static int ecsvm_string_array_push(ecsvm_string_array_t *array, char *value)
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

static void ecsvm_string_array_free(ecsvm_string_array_t *array)
{
    size_t index;

    for (index = 0u; index < array->count; ++index) {
        free(array->items[index]);
    }
    free(array->items);
    memset(array, 0, sizeof(*array));
}

static int ecsvm_token_array_push(ecsvm_token_array_t *array, ecsvm_token_t token)
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

static int ecsvm_syntax_node_array_push(
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

static void ecsvm_syntax_node_add_child(
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

static int ecsvm_source_file_array_push(
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

static void ecsvm_source_file_free(ecsvm_source_file_t *file)
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

static void ecsvm_source_file_array_free(ecsvm_source_file_array_t *array)
{
    size_t index;

    for (index = 0u; index < array->count; ++index) {
        ecsvm_source_file_free(&array->items[index]);
    }
    free(array->items);
    memset(array, 0, sizeof(*array));
}

static int ecsvm_semantic_field_array_push(
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

static int ecsvm_semantic_attribute_push(ecsvm_semantic_struct_t *semantic_struct, char *attribute)
{
    if (!ecsvm_reserve_bytes(
            (void **)&semantic_struct->attributes,
            sizeof(*semantic_struct->attributes),
            &semantic_struct->attribute_capacity,
            semantic_struct->attribute_count + 1u
        )) {
        return 0;
    }

    semantic_struct->attributes[semantic_struct->attribute_count] = attribute;
    semantic_struct->attribute_count += 1u;
    return 1;
}

static int ecsvm_semantic_struct_array_push(
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

static void ecsvm_semantic_struct_free(ecsvm_semantic_struct_t *semantic_struct)
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
    }
    free(semantic_struct->fields);
    free(semantic_struct->attributes);
    memset(semantic_struct, 0, sizeof(*semantic_struct));
}

static void ecsvm_semantic_struct_array_free(ecsvm_semantic_struct_array_t *array)
{
    size_t index;

    for (index = 0u; index < array->count; ++index) {
        ecsvm_semantic_struct_free(&array->items[index]);
    }
    free(array->items);
    memset(array, 0, sizeof(*array));
}

static int ecsvm_semantic_function_parameter_push(
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

static int ecsvm_semantic_function_array_push(
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

static void ecsvm_semantic_parameter_free(ecsvm_semantic_parameter_t *parameter)
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

static void ecsvm_semantic_function_free(ecsvm_semantic_function_t *function)
{
    size_t index;

    free(function->namespace_name);
    free(function->name);
    free(function->qualified_name);
    free(function->return_type_name);
    free(function->body_ast);
    for (index = 0u; index < function->parameter_count; ++index) {
        ecsvm_semantic_parameter_free(&function->parameters[index]);
    }
    for (index = 0u; index < function->attribute_count; ++index) {
        free(function->attributes[index]);
    }
    free(function->parameters);
    free(function->attributes);
    memset(function, 0, sizeof(*function));
}

static void ecsvm_semantic_function_array_free(ecsvm_semantic_function_array_t *array)
{
    size_t index;

    for (index = 0u; index < array->count; ++index) {
        ecsvm_semantic_function_free(&array->items[index]);
    }
    free(array->items);
    memset(array, 0, sizeof(*array));
}

static int ecsvm_blob_array_push(ecsvm_blob_array_t *array, ecsvm_blob_entry_t entry)
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

static void ecsvm_blob_array_free(ecsvm_blob_array_t *array)
{
    size_t index;

    for (index = 0u; index < array->count; ++index) {
        free(array->items[index].data);
    }
    free(array->items);
    memset(array, 0, sizeof(*array));
}

static int ecsvm_type_ref_builder_array_push(
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

static void ecsvm_type_ref_builder_array_free(ecsvm_type_ref_builder_array_t *array)
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

static int ecsvm_field_ref_builder_array_push(
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

static void ecsvm_field_ref_builder_array_free(ecsvm_field_ref_builder_array_t *array)
{
    size_t index;

    for (index = 0u; index < array->count; ++index) {
        free(array->items[index].name);
    }
    free(array->items);
    memset(array, 0, sizeof(*array));
}

static int ecsvm_field_def_builder_array_push(
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

static void ecsvm_field_def_builder_array_free(ecsvm_field_def_builder_array_t *array)
{
    free(array->items);
    memset(array, 0, sizeof(*array));
}

static int ecsvm_function_ref_builder_array_push(
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

static void ecsvm_function_ref_builder_array_free(ecsvm_function_ref_builder_array_t *array)
{
    size_t index;

    for (index = 0u; index < array->count; ++index) {
        free(array->items[index].namespace_name);
        free(array->items[index].name);
    }
    free(array->items);
    memset(array, 0, sizeof(*array));
}

static int ecsvm_parameter_builder_array_push(
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

static void ecsvm_parameter_builder_array_free(ecsvm_parameter_builder_array_t *array)
{
    size_t index;

    for (index = 0u; index < array->count; ++index) {
        free(array->items[index].name);
    }
    free(array->items);
    memset(array, 0, sizeof(*array));
}

static int ecsvm_attribute_builder_array_push(
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

static void ecsvm_attribute_builder_array_free(ecsvm_attribute_builder_array_t *array)
{
    free(array->items);
    memset(array, 0, sizeof(*array));
}

static int ecsvm_struct_def_builder_array_push(
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

static void ecsvm_struct_def_builder_array_free(ecsvm_struct_def_builder_array_t *array)
{
    free(array->items);
    memset(array, 0, sizeof(*array));
}

static int ecsvm_ast_node_array_push(
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

static void ecsvm_ast_node_add_child(
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

static void ecsvm_manifest_free(ecsvm_manifest_t *manifest)
{
    free(manifest->name);
    free(manifest->version);
    free(manifest->entry);
    memset(manifest, 0, sizeof(*manifest));
}

int ecsvm_path_is_directory(const char *path)
{
#ifdef _WIN32
    DWORD attributes;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat status;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    return stat(path, &status) == 0 && S_ISDIR(status.st_mode);
#endif
}

static int ecsvm_path_exists(const char *path)
{
#ifdef _WIN32
    DWORD attributes;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES;
#else
    struct stat status;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    return stat(path, &status) == 0;
#endif
}

int ecsvm_path_has_extension(const char *path, const char *extension)
{
    const char *dot;

    if (path == NULL || extension == NULL) {
        return 0;
    }

    dot = strrchr(path, '.');
    return dot != NULL && ecsvm_stricmp(dot, extension) == 0;
}

static int ecsvm_path_join(
    const char *left,
    const char *right,
    char *buffer,
    size_t buffer_capacity
)
{
    size_t left_length;
    const char *right_part;

    if (left == NULL || right == NULL || buffer == NULL || buffer_capacity == 0u) {
        return 0;
    }

    left_length = strlen(left);
    right_part = right;
    while (*right_part == '\\' || *right_part == '/') {
        right_part += 1;
    }

    if (left_length > 0u &&
        (left[left_length - 1u] == '\\' || left[left_length - 1u] == '/')) {
        return snprintf(buffer, buffer_capacity, "%s%s", left, right_part) > 0;
    }

    return snprintf(buffer, buffer_capacity, "%s%c%s", left, ECSVM_PATH_SEPARATOR, right_part) > 0;
}

static int ecsvm_read_text_file(const char *path, char **out_text, size_t *out_length)
{
    FILE *file;
    long length;
    char *text;

    file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }

    length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }

    text = (char *)malloc((size_t)length + 1u);
    if (text == NULL) {
        fclose(file);
        return 0;
    }

    if (length > 0 && fread(text, 1u, (size_t)length, file) != (size_t)length) {
        free(text);
        fclose(file);
        return 0;
    }

    fclose(file);
    text[length] = '\0';
    *out_text = text;
    if (out_length != NULL) {
        *out_length = (size_t)length;
    }
    return 1;
}

static int ecsvm_manifest_extract_value(
    const char *text,
    const char *key,
    char **out_value
)
{
    const char *cursor;
    size_t key_length;

    key_length = strlen(key);
    cursor = text;
    while (cursor != NULL && *cursor != '\0') {
        const char *line_end;
        const char *value_start;
        const char *value_end;

        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
            cursor += 1;
        }

        if (*cursor == '#') {
            line_end = strchr(cursor, '\n');
            cursor = line_end == NULL ? cursor + strlen(cursor) : line_end + 1;
            continue;
        }

        if (strncmp(cursor, key, key_length) != 0) {
            line_end = strchr(cursor, '\n');
            cursor = line_end == NULL ? cursor + strlen(cursor) : line_end + 1;
            continue;
        }

        cursor += key_length;
        while (*cursor == ' ' || *cursor == '\t') {
            cursor += 1;
        }
        if (*cursor != '=') {
            return 0;
        }

        cursor += 1;
        while (*cursor == ' ' || *cursor == '\t') {
            cursor += 1;
        }
        if (*cursor != '"') {
            return 0;
        }

        value_start = cursor + 1;
        value_end = strchr(value_start, '"');
        if (value_end == NULL) {
            return 0;
        }

        *out_value = ecsvm_copy_string_range(value_start, (size_t)(value_end - value_start));
        return *out_value != NULL;
    }

    return 0;
}

static int ecsvm_parse_manifest(
    const char *project_path,
    ecsvm_manifest_t *manifest,
    char *error_message,
    size_t error_message_capacity
)
{
    char manifest_path[MAX_PATH];
    char *text;

    if (!ecsvm_path_join(project_path, "project.toml", manifest_path, sizeof(manifest_path))) {
        ecsvm_set_error(error_message, error_message_capacity, "project.toml path is too long");
        return 0;
    }

    text = NULL;
    if (!ecsvm_read_text_file(manifest_path, &text, NULL)) {
        ecsvm_set_error(error_message, error_message_capacity, "failed to read project.toml");
        return 0;
    }

    if (!ecsvm_manifest_extract_value(text, "name", &manifest->name) ||
        !ecsvm_manifest_extract_value(text, "version", &manifest->version) ||
        !ecsvm_manifest_extract_value(text, "entry", &manifest->entry)) {
        free(text);
        ecsvm_manifest_free(manifest);
        ecsvm_set_error(error_message, error_message_capacity, "project.toml must define name, version, and entry");
        return 0;
    }

    free(text);
    return 1;
}

static int ecsvm_collect_ecs_files_recursive(
    const char *directory,
    ecsvm_string_array_t *paths,
    char *error_message,
    size_t error_message_capacity
)
{
#ifdef _WIN32
    char search_pattern[MAX_PATH];
    WIN32_FIND_DATAA find_data;
    HANDLE handle;

    if (!ecsvm_path_join(directory, "*", search_pattern, sizeof(search_pattern))) {
        ecsvm_set_error(error_message, error_message_capacity, "source path is too long");
        return 0;
    }

    handle = FindFirstFileA(search_pattern, &find_data);
    if (handle == INVALID_HANDLE_VALUE) {
        return 1;
    }

    do {
        char path[MAX_PATH];

        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        if (!ecsvm_path_join(directory, find_data.cFileName, path, sizeof(path))) {
            FindClose(handle);
            ecsvm_set_error(error_message, error_message_capacity, "source path is too long");
            return 0;
        }

        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if (!ecsvm_collect_ecs_files_recursive(path, paths, error_message, error_message_capacity)) {
                FindClose(handle);
                return 0;
            }
        } else if (ecsvm_path_has_extension(path, ".ecs")) {
            char *copy;

            copy = ecsvm_copy_string(path);
            if (copy == NULL || !ecsvm_string_array_push(paths, copy)) {
                free(copy);
                FindClose(handle);
                ecsvm_set_error(error_message, error_message_capacity, "out of memory while collecting source files");
                return 0;
            }
        }
    } while (FindNextFileA(handle, &find_data));

    FindClose(handle);
    return 1;
#else
    DIR *dir;
    struct dirent *entry;

    dir = opendir(directory);
    if (dir == NULL) {
        return 1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char path[MAX_PATH];
        struct stat status;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (!ecsvm_path_join(directory, entry->d_name, path, sizeof(path))) {
            closedir(dir);
            ecsvm_set_error(error_message, error_message_capacity, "source path is too long");
            return 0;
        }

        if (stat(path, &status) != 0) {
            continue;
        }

        if (S_ISDIR(status.st_mode)) {
            if (!ecsvm_collect_ecs_files_recursive(path, paths, error_message, error_message_capacity)) {
                closedir(dir);
                return 0;
            }
        } else if (ecsvm_path_has_extension(path, ".ecs")) {
            char *copy;

            copy = ecsvm_copy_string(path);
            if (copy == NULL || !ecsvm_string_array_push(paths, copy)) {
                free(copy);
                closedir(dir);
                ecsvm_set_error(error_message, error_message_capacity, "out of memory while collecting source files");
                return 0;
            }
        }
    }

    closedir(dir);
    return 1;
#endif
}

static int ecsvm_compare_strings(const void *left, const void *right)
{
    const char *const *left_text;
    const char *const *right_text;

    left_text = (const char *const *)left;
    right_text = (const char *const *)right;
    return ecsvm_stricmp(*left_text, *right_text);
}

static int ecsvm_is_identifier_start(int ch)
{
    return (ch >= 'A' && ch <= 'Z') ||
        (ch >= 'a' && ch <= 'z') ||
        ch == '_';
}

static int ecsvm_is_identifier_continue(int ch)
{
    return ecsvm_is_identifier_start(ch) || (ch >= '0' && ch <= '9');
}

static ecsvm_token_kind_t ecsvm_keyword_kind(const char *text, size_t length)
{
    if (length == 6u && memcmp(text, "import", 6u) == 0) {
        return ECSVM_TOKEN_KEY_IMPORT;
    }
    if (length == 9u && memcmp(text, "namespace", 9u) == 0) {
        return ECSVM_TOKEN_KEY_NAMESPACE;
    }
    if (length == 6u && memcmp(text, "struct", 6u) == 0) {
        return ECSVM_TOKEN_KEY_STRUCT;
    }
    if (length == 9u && memcmp(text, "component", 9u) == 0) {
        return ECSVM_TOKEN_KEY_COMPONENT;
    }
    if (length == 6u && memcmp(text, "system", 6u) == 0) {
        return ECSVM_TOKEN_KEY_SYSTEM;
    }
    if (length == 5u && memcmp(text, "const", 5u) == 0) {
        return ECSVM_TOKEN_KEY_CONST;
    }
    if (length == 2u && memcmp(text, "fn", 2u) == 0) {
        return ECSVM_TOKEN_KEY_FN;
    }

    return ECSVM_TOKEN_IDENTIFIER;
}

static int ecsvm_lex_source(
    ecsvm_source_file_t *file,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t offset;
    size_t line;
    size_t column;

    offset = 0u;
    line = 1u;
    column = 1u;
    while (offset < file->length) {
        ecsvm_token_t token;
        int ch;

        ch = (unsigned char)file->source[offset];
        if (ch == ' ' || ch == '\t' || ch == '\r') {
            offset += 1u;
            column += 1u;
            continue;
        }
        if (ch == '\n') {
            offset += 1u;
            line += 1u;
            column = 1u;
            continue;
        }
        if (ch == '/' && offset + 1u < file->length && file->source[offset + 1u] == '/') {
            offset += 2u;
            column += 2u;
            while (offset < file->length && file->source[offset] != '\n') {
                offset += 1u;
                column += 1u;
            }
            continue;
        }
        if (ch == '/' && offset + 1u < file->length && file->source[offset + 1u] == '*') {
            offset += 2u;
            column += 2u;
            while (offset + 1u < file->length &&
                   !(file->source[offset] == '*' && file->source[offset + 1u] == '/')) {
                if (file->source[offset] == '\n') {
                    line += 1u;
                    column = 1u;
                } else {
                    column += 1u;
                }
                offset += 1u;
            }
            if (offset + 1u >= file->length) {
                ecsvm_set_error(error_message, error_message_capacity, "unterminated block comment");
                return 0;
            }
            offset += 2u;
            column += 2u;
            continue;
        }

        memset(&token, 0, sizeof(token));
        token.offset = offset;
        token.line = line;
        token.column = column;
        token.length = 1u;

        if (ecsvm_is_identifier_start(ch)) {
            size_t start;

            start = offset;
            while (offset < file->length && ecsvm_is_identifier_continue((unsigned char)file->source[offset])) {
                offset += 1u;
                column += 1u;
            }
            token.kind = ecsvm_keyword_kind(file->source + start, offset - start);
            token.offset = start;
            token.length = offset - start;
        } else if (ch >= '0' && ch <= '9') {
            size_t start;

            start = offset;
            while (offset < file->length) {
                int digit;

                digit = (unsigned char)file->source[offset];
                if ((digit < '0' || digit > '9') && digit != '.') {
                    break;
                }
                offset += 1u;
                column += 1u;
            }
            token.kind = ECSVM_TOKEN_NUMBER;
            token.offset = start;
            token.length = offset - start;
        } else if (ch == '"') {
            size_t start;

            start = offset;
            offset += 1u;
            column += 1u;
            while (offset < file->length && file->source[offset] != '"') {
                if (file->source[offset] == '\n') {
                    ecsvm_set_error(error_message, error_message_capacity, "unterminated string literal");
                    return 0;
                }
                if (file->source[offset] == '\\' && offset + 1u < file->length) {
                    offset += 2u;
                    column += 2u;
                } else {
                    offset += 1u;
                    column += 1u;
                }
            }
            if (offset >= file->length) {
                ecsvm_set_error(error_message, error_message_capacity, "unterminated string literal");
                return 0;
            }
            offset += 1u;
            column += 1u;
            token.kind = ECSVM_TOKEN_STRING;
            token.offset = start;
            token.length = offset - start;
        } else {
            switch (ch) {
            case '{':
                token.kind = ECSVM_TOKEN_LBRACE;
                break;
            case '}':
                token.kind = ECSVM_TOKEN_RBRACE;
                break;
            case '[':
                token.kind = ECSVM_TOKEN_LBRACKET;
                break;
            case ']':
                token.kind = ECSVM_TOKEN_RBRACKET;
                break;
            case '(':
                token.kind = ECSVM_TOKEN_LPAREN;
                break;
            case ')':
                token.kind = ECSVM_TOKEN_RPAREN;
                break;
            case ':':
                token.kind = ECSVM_TOKEN_COLON;
                break;
            case ';':
                token.kind = ECSVM_TOKEN_SEMICOLON;
                break;
            case '.':
                token.kind = ECSVM_TOKEN_DOT;
                break;
            case ',':
                token.kind = ECSVM_TOKEN_COMMA;
                break;
            case '=':
                token.kind = ECSVM_TOKEN_EQUAL;
                break;
            case '!':
                token.kind = ECSVM_TOKEN_BANG;
                break;
            case '+':
                token.kind = ECSVM_TOKEN_PLUS;
                break;
            case '-':
                token.kind = ECSVM_TOKEN_MINUS;
                break;
            case '*':
                token.kind = ECSVM_TOKEN_STAR;
                break;
            case '/':
                token.kind = ECSVM_TOKEN_SLASH;
                break;
            case '%':
                token.kind = ECSVM_TOKEN_PERCENT;
                break;
            case '<':
                token.kind = ECSVM_TOKEN_LT;
                break;
            case '>':
                token.kind = ECSVM_TOKEN_GT;
                break;
            case '&':
                token.kind = ECSVM_TOKEN_AMPERSAND;
                break;
            case '|':
                token.kind = ECSVM_TOKEN_PIPE;
                break;
            case '^':
                token.kind = ECSVM_TOKEN_CARET;
                break;
            case '~':
                token.kind = ECSVM_TOKEN_TILDE;
                break;
            default:
                ecsvm_set_error(error_message, error_message_capacity, "unexpected character in source file");
                return 0;
            }

            offset += 1u;
            column += 1u;
        }

        if (!ecsvm_token_array_push(&file->tokens, token)) {
            ecsvm_set_error(error_message, error_message_capacity, "out of memory while lexing");
            return 0;
        }
    }

    return ecsvm_token_array_push(
        &file->tokens,
        (ecsvm_token_t){ ECSVM_TOKEN_EOF, file->length, 0u, line, column }
    );
}

static const ecsvm_token_t *ecsvm_parser_current(const ecsvm_parser_t *parser)
{
    return &parser->file->tokens.items[parser->index];
}

static int ecsvm_parser_match(ecsvm_parser_t *parser, ecsvm_token_kind_t kind)
{
    if (ecsvm_parser_current(parser)->kind != kind) {
        return 0;
    }

    parser->index += 1u;
    return 1;
}

static int ecsvm_parser_expect(
    ecsvm_parser_t *parser,
    ecsvm_token_kind_t kind,
    char *error_message,
    size_t error_message_capacity
)
{
    if (ecsvm_parser_match(parser, kind)) {
        return 1;
    }

    ecsvm_set_error(error_message, error_message_capacity, "unexpected token while parsing");
    return 0;
}

static int ecsvm_parser_parse_qualified_name(
    ecsvm_parser_t *parser,
    size_t *out_start,
    size_t *out_end,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t start;
    size_t end;

    if (ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_IDENTIFIER) {
        ecsvm_set_error(error_message, error_message_capacity, "expected identifier");
        return 0;
    }

    start = parser->index;
    end = parser->index;
    parser->index += 1u;
    while (ecsvm_parser_match(parser, ECSVM_TOKEN_DOT)) {
        if (ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_IDENTIFIER) {
            ecsvm_set_error(error_message, error_message_capacity, "expected identifier after '.'");
            return 0;
        }
        end = parser->index;
        parser->index += 1u;
    }

    *out_start = start;
    *out_end = end;
    return 1;
}

static int ecsvm_parser_parse_identifier(
    ecsvm_parser_t *parser,
    size_t *out_start,
    size_t *out_end,
    char *error_message,
    size_t error_message_capacity
)
{
    if (ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_IDENTIFIER) {
        ecsvm_set_error(error_message, error_message_capacity, "expected identifier");
        return 0;
    }

    *out_start = parser->index;
    *out_end = parser->index;
    parser->index += 1u;
    return 1;
}

static int ecsvm_parser_find_matching_token(
    const ecsvm_parser_t *parser,
    size_t start_index,
    ecsvm_token_kind_t open_kind,
    ecsvm_token_kind_t close_kind,
    size_t *out_end_index
)
{
    size_t depth;
    size_t index;

    if (parser->file->tokens.items[start_index].kind != open_kind) {
        return 0;
    }

    depth = 1u;
    index = start_index + 1u;
    while (index < parser->file->tokens.count) {
        ecsvm_token_kind_t kind;

        kind = parser->file->tokens.items[index].kind;
        if (kind == open_kind) {
            depth += 1u;
        } else if (kind == close_kind) {
            depth -= 1u;
            if (depth == 0u) {
                *out_end_index = index;
                return 1;
            }
        }
        index += 1u;
    }

    return 0;
}

static int ecsvm_parser_skip_block(
    ecsvm_parser_t *parser,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t depth;

    if (!ecsvm_parser_expect(parser, ECSVM_TOKEN_LBRACE, error_message, error_message_capacity)) {
        return 0;
    }

    depth = 1u;
    while (depth > 0u && ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_EOF) {
        if (ecsvm_parser_match(parser, ECSVM_TOKEN_LBRACE)) {
            depth += 1u;
        } else if (ecsvm_parser_match(parser, ECSVM_TOKEN_RBRACE)) {
            depth -= 1u;
        } else {
            parser->index += 1u;
        }
    }

    if (depth != 0u) {
        ecsvm_set_error(error_message, error_message_capacity, "unterminated block");
        return 0;
    }

    return 1;
}

static int ecsvm_parser_skip_until_semicolon(
    ecsvm_parser_t *parser,
    char *error_message,
    size_t error_message_capacity
)
{
    while (ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_SEMICOLON &&
           ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_EOF) {
        parser->index += 1u;
    }

    if (!ecsvm_parser_expect(parser, ECSVM_TOKEN_SEMICOLON, error_message, error_message_capacity)) {
        return 0;
    }

    return 1;
}

static int ecsvm_parser_parse_import(
    ecsvm_parser_t *parser,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t start;
    size_t end;

    if (!ecsvm_parser_parse_qualified_name(
            parser,
            &start,
            &end,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    return ecsvm_parser_expect(parser, ECSVM_TOKEN_SEMICOLON, error_message, error_message_capacity);
}

static int ecsvm_parser_parse_struct_body(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t struct_index,
    char *error_message,
    size_t error_message_capacity
)
{
    if (!ecsvm_parser_expect(parser, ECSVM_TOKEN_LBRACE, error_message, error_message_capacity)) {
        return 0;
    }

    while (ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_RBRACE &&
           ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_EOF) {
        ecsvm_syntax_node_t field;
        size_t field_index;

        memset(&field, 0, sizeof(field));
        field.kind = ECSVM_SYNTAX_FIELD;
        field.token_start = parser->index;
        if (!ecsvm_parser_parse_qualified_name(
                parser,
                &field.name_start,
                &field.name_end,
                error_message,
                error_message_capacity
            ) ||
            !ecsvm_parser_expect(parser, ECSVM_TOKEN_COLON, error_message, error_message_capacity) ||
            !ecsvm_parser_parse_qualified_name(
                parser,
                &field.type_start,
                &field.type_end,
                error_message,
                error_message_capacity
            ) ||
            !ecsvm_parser_expect(parser, ECSVM_TOKEN_SEMICOLON, error_message, error_message_capacity)) {
            return 0;
        }
        field.token_end = parser->index - 1u;

        if (!ecsvm_syntax_node_array_push(nodes, field, &field_index)) {
            ecsvm_set_error(error_message, error_message_capacity, "out of memory while building syntax tree");
            return 0;
        }

        ecsvm_syntax_node_add_child(nodes, struct_index, field_index);
    }

    return ecsvm_parser_expect(parser, ECSVM_TOKEN_RBRACE, error_message, error_message_capacity);
}

static int ecsvm_parser_parse_parameter(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t function_index,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_syntax_node_t parameter;
    size_t parameter_index;

    memset(&parameter, 0, sizeof(parameter));
    parameter.kind = ECSVM_SYNTAX_PARAMETER;
    parameter.token_start = parser->index;
    if (!ecsvm_parser_parse_identifier(
            parser,
            &parameter.name_start,
            &parameter.name_end,
            error_message,
            error_message_capacity
        ) ||
        !ecsvm_parser_expect(parser, ECSVM_TOKEN_COLON, error_message, error_message_capacity) ||
        !ecsvm_parser_parse_qualified_name(
            parser,
            &parameter.type_start,
            &parameter.type_end,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_EQUAL)) {
        size_t value_start;
        size_t value_end;
        size_t depth_paren;
        size_t depth_bracket;
        size_t depth_brace;

        value_start = parser->index;
        value_end = parser->index;
        depth_paren = 0u;
        depth_bracket = 0u;
        depth_brace = 0u;
        while (ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_EOF) {
            ecsvm_token_kind_t kind;

            kind = ecsvm_parser_current(parser)->kind;
            if (depth_paren == 0u &&
                depth_bracket == 0u &&
                depth_brace == 0u &&
                (kind == ECSVM_TOKEN_COMMA || kind == ECSVM_TOKEN_RPAREN)) {
                break;
            }

            if (kind == ECSVM_TOKEN_LPAREN) {
                depth_paren += 1u;
            } else if (kind == ECSVM_TOKEN_RPAREN) {
                if (depth_paren == 0u) {
                    break;
                }
                depth_paren -= 1u;
            } else if (kind == ECSVM_TOKEN_LBRACKET) {
                depth_bracket += 1u;
            } else if (kind == ECSVM_TOKEN_RBRACKET) {
                if (depth_bracket == 0u) {
                    break;
                }
                depth_bracket -= 1u;
            } else if (kind == ECSVM_TOKEN_LBRACE) {
                depth_brace += 1u;
            } else if (kind == ECSVM_TOKEN_RBRACE) {
                if (depth_brace == 0u) {
                    break;
                }
                depth_brace -= 1u;
            }

            value_end = parser->index;
            parser->index += 1u;
        }

        if (value_start > value_end) {
            ecsvm_set_error(error_message, error_message_capacity, "expected default parameter value");
            return 0;
        }
        parameter.value_start = value_start;
        parameter.value_end = value_end;
    }

    parameter.token_end = parser->index == 0u ? 0u : parser->index - 1u;
    if (!ecsvm_syntax_node_array_push(nodes, parameter, &parameter_index)) {
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while building syntax tree");
        return 0;
    }

    ecsvm_syntax_node_add_child(nodes, function_index, parameter_index);
    return 1;
}

static int ecsvm_parser_parse_function(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t parent_index,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_syntax_node_t function_node;
    size_t function_index;

    memset(&function_node, 0, sizeof(function_node));
    function_node.kind = ECSVM_SYNTAX_FUNCTION;
    function_node.token_start = parser->index - 1u;
    if (!ecsvm_parser_parse_identifier(
            parser,
            &function_node.name_start,
            &function_node.name_end,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    if (!ecsvm_syntax_node_array_push(nodes, function_node, &function_index)) {
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while building syntax tree");
        return 0;
    }

    ecsvm_syntax_node_add_child(nodes, parent_index, function_index);
    if (!ecsvm_parser_expect(parser, ECSVM_TOKEN_LPAREN, error_message, error_message_capacity)) {
        return 0;
    }

    while (ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_RPAREN &&
           ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_EOF) {
        if (!ecsvm_parser_parse_parameter(
                parser,
                nodes,
                function_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }
        if (!ecsvm_parser_match(parser, ECSVM_TOKEN_COMMA)) {
            break;
        }
    }

    if (!ecsvm_parser_expect(parser, ECSVM_TOKEN_RPAREN, error_message, error_message_capacity)) {
        return 0;
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_COLON) &&
        !ecsvm_parser_parse_qualified_name(
            parser,
            &nodes->items[function_index].type_start,
            &nodes->items[function_index].type_end,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_SEMICOLON)) {
        nodes->items[function_index].token_end = parser->index - 1u;
        return 1;
    }

    if (ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_LBRACE ||
        !ecsvm_parser_find_matching_token(
            parser,
            parser->index,
            ECSVM_TOKEN_LBRACE,
            ECSVM_TOKEN_RBRACE,
            &nodes->items[function_index].body_end
        )) {
        ecsvm_set_error(error_message, error_message_capacity, "unterminated function body");
        return 0;
    }

    nodes->items[function_index].has_body = 1;
    nodes->items[function_index].body_start = parser->index;
    parser->index = nodes->items[function_index].body_end + 1u;
    nodes->items[function_index].token_end = parser->index - 1u;
    return 1;
}

static int ecsvm_parser_parse_declaration(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t parent_index,
    char *error_message,
    size_t error_message_capacity
);

static int ecsvm_parser_parse_namespace(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t parent_index,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_syntax_node_t namespace_node;
    size_t namespace_index;

    memset(&namespace_node, 0, sizeof(namespace_node));
    namespace_node.kind = ECSVM_SYNTAX_NAMESPACE;
    namespace_node.token_start = parser->index - 1u;
    if (!ecsvm_parser_parse_qualified_name(
            parser,
            &namespace_node.name_start,
            &namespace_node.name_end,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    if (!ecsvm_syntax_node_array_push(nodes, namespace_node, &namespace_index)) {
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while building syntax tree");
        return 0;
    }

    ecsvm_syntax_node_add_child(nodes, parent_index, namespace_index);
    if (!ecsvm_parser_expect(parser, ECSVM_TOKEN_LBRACE, error_message, error_message_capacity)) {
        return 0;
    }

    while (ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_RBRACE &&
           ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_EOF) {
        if (!ecsvm_parser_parse_declaration(
                parser,
                nodes,
                namespace_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }
    }

    namespace_node = nodes->items[namespace_index];
    namespace_node.token_end = parser->index;
    nodes->items[namespace_index] = namespace_node;
    return ecsvm_parser_expect(parser, ECSVM_TOKEN_RBRACE, error_message, error_message_capacity);
}

static int ecsvm_parser_add_component_attribute(
    ecsvm_syntax_node_array_t *nodes,
    size_t struct_index,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_syntax_node_t attribute;
    size_t attribute_index;

    memset(&attribute, 0, sizeof(attribute));
    attribute.kind = ECSVM_SYNTAX_ATTRIBUTE;
    attribute.token_start = 0u;
    attribute.token_end = 0u;
    attribute.name_start = 0u;
    attribute.name_end = 0u;
    if (!ecsvm_syntax_node_array_push(nodes, attribute, &attribute_index)) {
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while building syntax tree");
        return 0;
    }

    nodes->items[struct_index].is_component = 1;
    ecsvm_syntax_node_add_child(nodes, struct_index, attribute_index);
    return 1;
}

static int ecsvm_parser_parse_struct_like(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t parent_index,
    int implicit_component,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_syntax_node_t struct_node;
    size_t struct_index;

    memset(&struct_node, 0, sizeof(struct_node));
    struct_node.kind = ECSVM_SYNTAX_STRUCT;
    struct_node.token_start = parser->index - 1u;
    struct_node.is_component = implicit_component;
    if (!ecsvm_parser_parse_qualified_name(
            parser,
            &struct_node.name_start,
            &struct_node.name_end,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    if (!ecsvm_syntax_node_array_push(nodes, struct_node, &struct_index)) {
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while building syntax tree");
        return 0;
    }

    ecsvm_syntax_node_add_child(nodes, parent_index, struct_index);
    if (implicit_component &&
        !ecsvm_parser_add_component_attribute(nodes, struct_index, error_message, error_message_capacity)) {
        return 0;
    }

    if (!ecsvm_parser_parse_struct_body(
            parser,
            nodes,
            struct_index,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    nodes->items[struct_index].token_end = parser->index - 1u;
    return 1;
}

static int ecsvm_parser_parse_attribute(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t struct_index,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_syntax_node_t attribute;
    size_t attribute_index;

    memset(&attribute, 0, sizeof(attribute));
    attribute.kind = ECSVM_SYNTAX_ATTRIBUTE;
    attribute.token_start = parser->index;
    if (!ecsvm_parser_parse_qualified_name(
            parser,
            &attribute.name_start,
            &attribute.name_end,
            error_message,
            error_message_capacity
        ) ||
        !ecsvm_parser_expect(parser, ECSVM_TOKEN_RBRACKET, error_message, error_message_capacity)) {
        return 0;
    }

    attribute.token_end = parser->index - 1u;
    if (!ecsvm_syntax_node_array_push(nodes, attribute, &attribute_index)) {
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while building syntax tree");
        return 0;
    }

    ecsvm_syntax_node_add_child(nodes, struct_index, attribute_index);
    return 1;
}

static int ecsvm_parser_parse_declaration(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t parent_index,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t attribute_start;
    size_t struct_index;
    ecsvm_syntax_node_t struct_node;

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_KEY_IMPORT)) {
        return ecsvm_parser_parse_import(parser, error_message, error_message_capacity);
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_KEY_NAMESPACE)) {
        return ecsvm_parser_parse_namespace(
            parser,
            nodes,
            parent_index,
            error_message,
            error_message_capacity
        );
    }

    attribute_start = parser->index;
    if (ecsvm_parser_match(parser, ECSVM_TOKEN_LBRACKET)) {
        memset(&struct_node, 0, sizeof(struct_node));
        struct_node.kind = ECSVM_SYNTAX_STRUCT;
        struct_node.token_start = attribute_start;

        if (ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_IDENTIFIER) {
            ecsvm_set_error(error_message, error_message_capacity, "expected attribute name");
            return 0;
        }

        if (!ecsvm_syntax_node_array_push(nodes, struct_node, &struct_index)) {
            ecsvm_set_error(error_message, error_message_capacity, "out of memory while building syntax tree");
            return 0;
        }

        while (1) {
            ecsvm_syntax_node_add_child(nodes, parent_index, struct_index);
            if (!ecsvm_parser_parse_attribute(
                    parser,
                    nodes,
                    struct_index,
                    error_message,
                    error_message_capacity
                )) {
                return 0;
            }
            if (!ecsvm_parser_match(parser, ECSVM_TOKEN_LBRACKET)) {
                break;
            }
        }

        if (!ecsvm_parser_expect(parser, ECSVM_TOKEN_KEY_STRUCT, error_message, error_message_capacity) ||
            !ecsvm_parser_parse_qualified_name(
                parser,
                &nodes->items[struct_index].name_start,
                &nodes->items[struct_index].name_end,
                error_message,
                error_message_capacity
            ) ||
            !ecsvm_parser_parse_struct_body(
                parser,
                nodes,
                struct_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }

        nodes->items[struct_index].token_end = parser->index - 1u;
        return 1;
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_KEY_STRUCT)) {
        return ecsvm_parser_parse_struct_like(
            parser,
            nodes,
            parent_index,
            0,
            error_message,
            error_message_capacity
        );
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_KEY_COMPONENT)) {
        return ecsvm_parser_parse_struct_like(
            parser,
            nodes,
            parent_index,
            1,
            error_message,
            error_message_capacity
        );
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_KEY_FN)) {
        return ecsvm_parser_parse_function(
            parser,
            nodes,
            parent_index,
            error_message,
            error_message_capacity
        );
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_KEY_SYSTEM)) {
        while (ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_LBRACE &&
               ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_EOF) {
            parser->index += 1u;
        }
        return ecsvm_parser_skip_block(parser, error_message, error_message_capacity);
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_KEY_CONST)) {
        return ecsvm_parser_skip_until_semicolon(parser, error_message, error_message_capacity);
    }

    ecsvm_set_error(error_message, error_message_capacity, "unsupported declaration in source");
    return 0;
}

static int ecsvm_parse_file(
    ecsvm_source_file_t *file,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_parser_t parser;
    ecsvm_syntax_node_t root;
    ecsvm_syntax_node_t file_node;
    size_t root_index;
    size_t file_index;

    memset(&parser, 0, sizeof(parser));
    parser.file = file;
    parser.index = 0u;

    memset(&root, 0, sizeof(root));
    root.kind = ECSVM_SYNTAX_ROOT;
    if (!ecsvm_syntax_node_array_push(&file->nodes, root, &root_index)) {
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while building syntax tree");
        return 0;
    }

    memset(&file_node, 0, sizeof(file_node));
    file_node.kind = ECSVM_SYNTAX_FILE;
    if (!ecsvm_syntax_node_array_push(&file->nodes, file_node, &file_index)) {
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while building syntax tree");
        return 0;
    }

    ecsvm_syntax_node_add_child(&file->nodes, root_index, file_index);
    while (ecsvm_parser_current(&parser)->kind != ECSVM_TOKEN_EOF) {
        if (!ecsvm_parser_parse_declaration(
                &parser,
                &file->nodes,
                file_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }
    }

    return 1;
}

static char *ecsvm_tokens_to_name(
    const ecsvm_source_file_t *file,
    size_t start,
    size_t end
)
{
    size_t token_index;
    size_t length;
    char *name;
    size_t write_offset;
    int first;

    length = 0u;
    for (token_index = start; token_index <= end; ++token_index) {
        const ecsvm_token_t *token;

        token = &file->tokens.items[token_index];
        if (token->kind != ECSVM_TOKEN_IDENTIFIER) {
            continue;
        }

        length += token->length;
        if (token_index != end) {
            length += 1u;
        }
    }

    name = (char *)malloc(length + 1u);
    if (name == NULL) {
        return NULL;
    }

    write_offset = 0u;
    first = 1;
    for (token_index = start; token_index <= end; ++token_index) {
        const ecsvm_token_t *token;

        token = &file->tokens.items[token_index];
        if (token->kind != ECSVM_TOKEN_IDENTIFIER) {
            continue;
        }

        if (!first) {
            name[write_offset] = '.';
            write_offset += 1u;
        }
        memcpy(name + write_offset, file->source + token->offset, token->length);
        write_offset += token->length;
        first = 0;
    }
    name[write_offset] = '\0';
    return name;
}

static char *ecsvm_tokens_to_source(
    const ecsvm_source_file_t *file,
    size_t start,
    size_t end
)
{
    size_t offset;
    size_t length;

    if (start > end) {
        return ecsvm_copy_string("");
    }

    offset = file->tokens.items[start].offset;
    length = file->tokens.items[end].offset + file->tokens.items[end].length - offset;
    return ecsvm_copy_string_range(file->source + offset, length);
}

static int ecsvm_ast_build_group(
    const ecsvm_source_file_t *file,
    size_t start,
    size_t end,
    ecsvm_ast_node_array_t *nodes,
    size_t parent_index,
    uint32_t group_kind,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t group_index;
    size_t cursor;

    if (!ecsvm_ast_node_array_push(
            nodes,
            (ecsvm_ast_node_t){ group_kind, 0u, 0u, 0u, 0u, 0u, 0u },
            &group_index
        )) {
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while building function ast");
        return 0;
    }

    ecsvm_ast_node_add_child(nodes, parent_index, group_index);
    cursor = start + 1u;
    while (cursor < end) {
        ecsvm_token_kind_t kind;

        kind = file->tokens.items[cursor].kind;
        if (kind == ECSVM_TOKEN_LBRACE ||
            kind == ECSVM_TOKEN_LPAREN ||
            kind == ECSVM_TOKEN_LBRACKET) {
            size_t matching_end;
            uint32_t nested_kind;
            ecsvm_token_kind_t close_kind;

            if (kind == ECSVM_TOKEN_LBRACE) {
                nested_kind = ECSVM_AST_NODE_BLOCK;
                close_kind = ECSVM_TOKEN_RBRACE;
            } else if (kind == ECSVM_TOKEN_LPAREN) {
                nested_kind = ECSVM_AST_NODE_GROUP_PAREN;
                close_kind = ECSVM_TOKEN_RPAREN;
            } else {
                nested_kind = ECSVM_AST_NODE_GROUP_BRACKET;
                close_kind = ECSVM_TOKEN_RBRACKET;
            }

            if (!ecsvm_parser_find_matching_token(
                    &(ecsvm_parser_t){ file, 0u },
                    cursor,
                    kind,
                    close_kind,
                    &matching_end
                ) ||
                matching_end > end ||
                !ecsvm_ast_build_group(
                    file,
                    cursor,
                    matching_end,
                    nodes,
                    group_index,
                    nested_kind,
                    error_message,
                    error_message_capacity
                )) {
                return 0;
            }
            cursor = matching_end + 1u;
            continue;
        }

        if (kind == ECSVM_TOKEN_RBRACE ||
            kind == ECSVM_TOKEN_RPAREN ||
            kind == ECSVM_TOKEN_RBRACKET) {
            ecsvm_set_error(error_message, error_message_capacity, "unexpected closing delimiter in function body");
            return 0;
        }

        {
            size_t node_index;

            if (!ecsvm_ast_node_array_push(
                    nodes,
                    (ecsvm_ast_node_t){
                        ECSVM_AST_NODE_TOKEN,
                        0u,
                        0u,
                        0u,
                        (uint32_t)kind,
                        (uint32_t)cursor,
                        (uint32_t)file->tokens.items[cursor].length
                    },
                    &node_index
                )) {
                ecsvm_set_error(error_message, error_message_capacity, "out of memory while building function ast");
                return 0;
            }

            ecsvm_ast_node_add_child(nodes, group_index, node_index);
        }

        cursor += 1u;
    }

    return 1;
}

static int ecsvm_build_function_ast_blob(
    const ecsvm_source_file_t *file,
    size_t body_start,
    size_t body_end,
    unsigned char **out_data,
    size_t *out_length,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_ast_node_array_t nodes;
    unsigned char *data;
    size_t index;
    size_t text_bytes;
    size_t offset;

    memset(&nodes, 0, sizeof(nodes));
    if (!ecsvm_ast_node_array_push(
            &nodes,
            (ecsvm_ast_node_t){ ECSVM_AST_NODE_ROOT, 0u, 0u, 0u, 0u, 0u, 0u },
            NULL
        ) ||
        !ecsvm_ast_build_group(
            file,
            body_start,
            body_end,
            &nodes,
            0u,
            ECSVM_AST_NODE_BLOCK,
            error_message,
            error_message_capacity
        )) {
        free(nodes.items);
        return 0;
    }

    text_bytes = 0u;
    for (index = 0u; index < nodes.count; ++index) {
        if (nodes.items[index].kind == ECSVM_AST_NODE_TOKEN) {
            text_bytes += nodes.items[index].text_length;
        }
    }

    *out_length = sizeof(uint32_t) * 2u + nodes.count * sizeof(ecsvm_ast_node_t) + text_bytes;
    data = (unsigned char *)malloc(*out_length);
    if (data == NULL) {
        free(nodes.items);
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while serializing function ast");
        return 0;
    }

    ((uint32_t *)data)[0] = 1u;
    ((uint32_t *)data)[1] = (uint32_t)nodes.count;
    memcpy(data + sizeof(uint32_t) * 2u, nodes.items, nodes.count * sizeof(ecsvm_ast_node_t));
    offset = sizeof(uint32_t) * 2u + nodes.count * sizeof(ecsvm_ast_node_t);
    for (index = 0u; index < nodes.count; ++index) {
        if (nodes.items[index].kind == ECSVM_AST_NODE_TOKEN) {
            ecsvm_ast_node_t *node;
            const ecsvm_token_t *token;

            node = &((ecsvm_ast_node_t *)(data + sizeof(uint32_t) * 2u))[index];
            token = &file->tokens.items[nodes.items[index].text_offset];
            node->text_offset = (uint32_t)(offset - (sizeof(uint32_t) * 2u + nodes.count * sizeof(ecsvm_ast_node_t)));
            memcpy(data + offset, file->source + token->offset, token->length);
            offset += token->length;
        }
    }

    free(nodes.items);
    *out_data = data;
    return 1;
}

static char *ecsvm_join_qualified_name(const char *namespace_name, const char *name)
{
    if (namespace_name == NULL || namespace_name[0] == '\0') {
        return ecsvm_copy_string(name);
    }

    {
        size_t namespace_length;
        size_t name_length;
        char *qualified_name;

        namespace_length = strlen(namespace_name);
        name_length = strlen(name);
        qualified_name = (char *)malloc(namespace_length + name_length + 2u);
        if (qualified_name == NULL) {
            return NULL;
        }

        (void)snprintf(
            qualified_name,
            namespace_length + name_length + 2u,
            "%s.%s",
            namespace_name,
            name
        );
        return qualified_name;
    }
}

static int ecsvm_is_builtin_alias(const char *name)
{
    return strcmp(name, "entity") == 0 ||
        strcmp(name, "i32") == 0 ||
        strcmp(name, "u32") == 0 ||
        strcmp(name, "f32") == 0 ||
        strcmp(name, "void") == 0 ||
        strcmp(name, "blob") == 0 ||
        strcmp(name, "string") == 0 ||
        strcmp(name, "bool") == 0;
}

static char *ecsvm_builtin_type_name(const char *alias)
{
    if (strcmp(alias, "entity") == 0) {
        return ecsvm_copy_string("core.Entity");
    }
    if (strcmp(alias, "i32") == 0) {
        return ecsvm_copy_string("core.Int32");
    }
    if (strcmp(alias, "u32") == 0) {
        return ecsvm_copy_string("core.UInt32");
    }
    if (strcmp(alias, "f32") == 0) {
        return ecsvm_copy_string("core.Float32");
    }
    if (strcmp(alias, "void") == 0) {
        return ecsvm_copy_string("core.Void");
    }
    if (strcmp(alias, "blob") == 0) {
        return ecsvm_copy_string("core.Blob");
    }
    if (strcmp(alias, "string") == 0) {
        return ecsvm_copy_string("core.String");
    }
    if (strcmp(alias, "bool") == 0) {
        return ecsvm_copy_string("core.Bool");
    }
    return NULL;
}

static size_t ecsvm_builtin_layout(const char *qualified_name, size_t *out_alignment)
{
    size_t size;
    size_t alignment;

    size = 0u;
    alignment = 0u;
    if (strcmp(qualified_name, "core.Entity") == 0) {
        size = sizeof(uint32_t);
        alignment = ECSVM_ALIGNOF(uint32_t);
    } else if (strcmp(qualified_name, "core.Int32") == 0) {
        size = sizeof(int32_t);
        alignment = ECSVM_ALIGNOF(int32_t);
    } else if (strcmp(qualified_name, "core.UInt32") == 0) {
        size = sizeof(uint32_t);
        alignment = ECSVM_ALIGNOF(uint32_t);
    } else if (strcmp(qualified_name, "core.Float32") == 0) {
        size = sizeof(float);
        alignment = ECSVM_ALIGNOF(float);
    } else if (strcmp(qualified_name, "core.Blob") == 0 ||
               strcmp(qualified_name, "core.String") == 0) {
        size = sizeof(uint32_t);
        alignment = ECSVM_ALIGNOF(uint32_t);
    } else if (strcmp(qualified_name, "core.Bool") == 0) {
        size = sizeof(unsigned char);
        alignment = ECSVM_ALIGNOF(unsigned char);
    }

    if (out_alignment != NULL) {
        *out_alignment = alignment;
    }
    return size;
}

static size_t ecsvm_align_up(size_t value, size_t alignment)
{
    if (alignment == 0u) {
        return value;
    }

    return (value + alignment - 1u) / alignment * alignment;
}

static int ecsvm_find_semantic_struct(
    const ecsvm_semantic_struct_array_t *semantic_structs,
    const char *qualified_name
)
{
    size_t index;

    for (index = 0u; index < semantic_structs->count; ++index) {
        if (strcmp(semantic_structs->items[index].qualified_name, qualified_name) == 0) {
            return (int)index;
        }
    }

    return -1;
}

static int ecsvm_find_semantic_function(
    const ecsvm_semantic_function_array_t *semantic_functions,
    const char *qualified_name
)
{
    size_t index;

    for (index = 0u; index < semantic_functions->count; ++index) {
        if (strcmp(semantic_functions->items[index].qualified_name, qualified_name) == 0) {
            return (int)index;
        }
    }

    return -1;
}

static int ecsvm_find_semantic_struct_by_suffix(
    const ecsvm_semantic_struct_array_t *semantic_structs,
    const char *name
)
{
    size_t index;
    size_t name_length;
    int match_index;

    name_length = strlen(name);
    match_index = -1;
    for (index = 0u; index < semantic_structs->count; ++index) {
        const char *qualified_name;
        size_t qualified_length;

        qualified_name = semantic_structs->items[index].qualified_name;
        qualified_length = strlen(qualified_name);
        if (strcmp(qualified_name, name) == 0 ||
            (qualified_length > name_length &&
             strcmp(qualified_name + qualified_length - name_length, name) == 0 &&
             qualified_name[qualified_length - name_length - 1u] == '.')) {
            if (match_index >= 0) {
                return -1;
            }
            match_index = (int)index;
        }
    }

    return match_index;
}

static char *ecsvm_resolve_type_name(
    const ecsvm_semantic_struct_array_t *semantic_structs,
    const char *current_namespace,
    const char *name
)
{
    char *qualified_name;
    int index;

    if (ecsvm_is_builtin_alias(name)) {
        return ecsvm_builtin_type_name(name);
    }

    index = ecsvm_find_semantic_struct(semantic_structs, name);
    if (index >= 0) {
        return ecsvm_copy_string(semantic_structs->items[index].qualified_name);
    }

    if (strchr(name, '.') != NULL) {
        index = ecsvm_find_semantic_struct_by_suffix(semantic_structs, name);
        if (index >= 0) {
            return ecsvm_copy_string(semantic_structs->items[index].qualified_name);
        }
        return ecsvm_copy_string(name);
    }

    qualified_name = ecsvm_join_qualified_name(current_namespace, name);
    if (qualified_name == NULL) {
        return NULL;
    }

    index = ecsvm_find_semantic_struct(semantic_structs, qualified_name);
    if (index >= 0) {
        return qualified_name;
    }

    free(qualified_name);
    index = ecsvm_find_semantic_struct_by_suffix(semantic_structs, name);
    if (index >= 0) {
        return ecsvm_copy_string(semantic_structs->items[index].qualified_name);
    }
    return ecsvm_copy_string(name);
}

static int ecsvm_collect_semantic_from_node(
    const ecsvm_source_file_t *file,
    const ecsvm_syntax_node_array_t *nodes,
    size_t node_index,
    const char *namespace_name,
    ecsvm_semantic_struct_array_t *semantic_structs,
    ecsvm_semantic_function_array_t *semantic_functions,
    char *error_message,
    size_t error_message_capacity
)
{
    const ecsvm_syntax_node_t *node;

    node = &nodes->items[node_index];
    if (node->kind == ECSVM_SYNTAX_NAMESPACE) {
        char *child_namespace;
        size_t child_index;

        child_namespace = ecsvm_tokens_to_name(file, node->name_start, node->name_end);
        if (child_namespace == NULL) {
            ecsvm_set_error(error_message, error_message_capacity, "out of memory while collecting namespaces");
            return 0;
        }

        for (child_index = node->first_child; child_index != 0u; child_index = nodes->items[child_index].next_sibling) {
            if (!ecsvm_collect_semantic_from_node(
                    file,
                    nodes,
                    child_index,
                    child_namespace,
                    semantic_structs,
                    semantic_functions,
                    error_message,
                    error_message_capacity
                )) {
                free(child_namespace);
                return 0;
            }
        }

        free(child_namespace);
        return 1;
    }

    if (node->kind == ECSVM_SYNTAX_STRUCT) {
        ecsvm_semantic_struct_t semantic_struct;
        size_t child_index;

        memset(&semantic_struct, 0, sizeof(semantic_struct));
        semantic_struct.namespace_name = ecsvm_copy_string(namespace_name != NULL ? namespace_name : "");
        semantic_struct.name = ecsvm_tokens_to_name(file, node->name_start, node->name_end);
        semantic_struct.qualified_name = ecsvm_join_qualified_name(
            semantic_struct.namespace_name,
            semantic_struct.name
        );
        semantic_struct.is_component = node->is_component;
        if (semantic_struct.namespace_name == NULL ||
            semantic_struct.name == NULL ||
            semantic_struct.qualified_name == NULL) {
            ecsvm_semantic_struct_free(&semantic_struct);
            ecsvm_set_error(error_message, error_message_capacity, "out of memory while collecting struct names");
            return 0;
        }

        if (ecsvm_find_semantic_struct(semantic_structs, semantic_struct.qualified_name) >= 0) {
            ecsvm_semantic_struct_free(&semantic_struct);
            ecsvm_set_error(error_message, error_message_capacity, "duplicate struct definition");
            return 0;
        }

        for (child_index = node->first_child; child_index != 0u; child_index = nodes->items[child_index].next_sibling) {
            const ecsvm_syntax_node_t *child;

            child = &nodes->items[child_index];
            if (child->kind == ECSVM_SYNTAX_ATTRIBUTE) {
                char *attribute_name;

                if (child->name_start == 0u && child->name_end == 0u) {
                    attribute_name = ecsvm_copy_string("core.Component");
                } else {
                    attribute_name = ecsvm_tokens_to_name(file, child->name_start, child->name_end);
                }

                if (attribute_name == NULL ||
                    !ecsvm_semantic_attribute_push(&semantic_struct, attribute_name)) {
                    free(attribute_name);
                    ecsvm_semantic_struct_free(&semantic_struct);
                    ecsvm_set_error(error_message, error_message_capacity, "out of memory while collecting attributes");
                    return 0;
                }

                if (strcmp(attribute_name, "core.Component") == 0) {
                    semantic_struct.is_component = 1;
                }
            } else if (child->kind == ECSVM_SYNTAX_FIELD) {
                ecsvm_semantic_field_t field;

                memset(&field, 0, sizeof(field));
                field.name = ecsvm_tokens_to_name(file, child->name_start, child->name_end);
                field.type_name = ecsvm_tokens_to_name(file, child->type_start, child->type_end);
                if (field.name == NULL || field.type_name == NULL ||
                    !ecsvm_semantic_field_array_push(&semantic_struct, field)) {
                    free(field.name);
                    free(field.type_name);
                    ecsvm_semantic_struct_free(&semantic_struct);
                    ecsvm_set_error(error_message, error_message_capacity, "out of memory while collecting fields");
                    return 0;
                }
            }
        }

        if (!ecsvm_semantic_struct_array_push(semantic_structs, semantic_struct)) {
            ecsvm_semantic_struct_free(&semantic_struct);
            ecsvm_set_error(error_message, error_message_capacity, "out of memory while collecting structs");
            return 0;
        }

        return 1;
    }

    if (node->kind == ECSVM_SYNTAX_FUNCTION) {
        ecsvm_semantic_function_t semantic_function;
        size_t child_index;

        memset(&semantic_function, 0, sizeof(semantic_function));
        semantic_function.namespace_name = ecsvm_copy_string(namespace_name != NULL ? namespace_name : "");
        semantic_function.name = ecsvm_tokens_to_name(file, node->name_start, node->name_end);
        semantic_function.qualified_name = ecsvm_join_qualified_name(
            semantic_function.namespace_name,
            semantic_function.name
        );
        semantic_function.return_type_name = (node->type_start != 0u || node->type_end != 0u)
            ? ecsvm_tokens_to_name(file, node->type_start, node->type_end)
            : ecsvm_copy_string("core.Void");
        if (semantic_function.namespace_name == NULL ||
            semantic_function.name == NULL ||
            semantic_function.qualified_name == NULL ||
            semantic_function.return_type_name == NULL) {
            ecsvm_semantic_function_free(&semantic_function);
            ecsvm_set_error(error_message, error_message_capacity, "out of memory while collecting function names");
            return 0;
        }

        if (ecsvm_find_semantic_function(semantic_functions, semantic_function.qualified_name) >= 0) {
            ecsvm_semantic_function_free(&semantic_function);
            ecsvm_set_error(error_message, error_message_capacity, "duplicate function definition");
            return 0;
        }

        for (child_index = node->first_child; child_index != 0u; child_index = nodes->items[child_index].next_sibling) {
            const ecsvm_syntax_node_t *child;

            child = &nodes->items[child_index];
            if (child->kind == ECSVM_SYNTAX_PARAMETER) {
                ecsvm_semantic_parameter_t parameter;

                memset(&parameter, 0, sizeof(parameter));
                parameter.name = ecsvm_tokens_to_name(file, child->name_start, child->name_end);
                parameter.type_name = ecsvm_tokens_to_name(file, child->type_start, child->type_end);
                parameter.default_value = (child->value_start != 0u || child->value_end != 0u)
                    ? ecsvm_tokens_to_source(file, child->value_start, child->value_end)
                    : NULL;
                if (parameter.name == NULL || parameter.type_name == NULL ||
                    !ecsvm_semantic_function_parameter_push(&semantic_function, parameter)) {
                    ecsvm_semantic_parameter_free(&parameter);
                    ecsvm_semantic_function_free(&semantic_function);
                    ecsvm_set_error(error_message, error_message_capacity, "out of memory while collecting parameters");
                    return 0;
                }
            }
        }

        if (node->has_body &&
            !ecsvm_build_function_ast_blob(
                file,
                node->body_start,
                node->body_end,
                &semantic_function.body_ast,
                &semantic_function.body_ast_length,
                error_message,
                error_message_capacity
            )) {
            ecsvm_semantic_function_free(&semantic_function);
            return 0;
        }

        if (!ecsvm_semantic_function_array_push(semantic_functions, semantic_function)) {
            ecsvm_semantic_function_free(&semantic_function);
            ecsvm_set_error(error_message, error_message_capacity, "out of memory while collecting functions");
            return 0;
        }
    }

    return 1;
}

static int ecsvm_collect_semantics(
    const ecsvm_source_file_array_t *files,
    ecsvm_semantic_struct_array_t *semantic_structs,
    ecsvm_semantic_function_array_t *semantic_functions,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t file_index;

    for (file_index = 0u; file_index < files->count; ++file_index) {
        const ecsvm_source_file_t *file;
        const ecsvm_syntax_node_t *file_node;
        size_t child_index;

        file = &files->items[file_index];
        file_node = &file->nodes.items[1];
        for (child_index = file_node->first_child; child_index != 0u; child_index = file->nodes.items[child_index].next_sibling) {
            if (!ecsvm_collect_semantic_from_node(
                    file,
                    &file->nodes,
                    child_index,
                    "",
                    semantic_structs,
                    semantic_functions,
                    error_message,
                    error_message_capacity
                )) {
                return 0;
            }
        }
    }

    return 1;
}

static int ecsvm_resolve_semantic_types(
    ecsvm_semantic_struct_array_t *semantic_structs,
    ecsvm_semantic_function_array_t *semantic_functions,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t struct_index;

    for (struct_index = 0u; struct_index < semantic_structs->count; ++struct_index) {
        ecsvm_semantic_struct_t *semantic_struct;
        size_t field_index;
        size_t attribute_index;

        semantic_struct = &semantic_structs->items[struct_index];
        for (field_index = 0u; field_index < semantic_struct->field_count; ++field_index) {
            char *resolved_type;

            resolved_type = ecsvm_resolve_type_name(
                semantic_structs,
                semantic_struct->namespace_name,
                semantic_struct->fields[field_index].type_name
            );
            if (resolved_type == NULL) {
                ecsvm_set_error(error_message, error_message_capacity, "out of memory while resolving field types");
                return 0;
            }

            free(semantic_struct->fields[field_index].type_name);
            semantic_struct->fields[field_index].type_name = resolved_type;
            if (ecsvm_builtin_layout(resolved_type, NULL) == 0u &&
                ecsvm_find_semantic_struct(semantic_structs, resolved_type) < 0) {
                ecsvm_set_error(error_message, error_message_capacity, "field type does not resolve");
                return 0;
            }
        }

        for (attribute_index = 0u; attribute_index < semantic_struct->attribute_count; ++attribute_index) {
            char *resolved_attribute;

            resolved_attribute = ecsvm_resolve_type_name(
                semantic_structs,
                semantic_struct->namespace_name,
                semantic_struct->attributes[attribute_index]
            );
            if (resolved_attribute == NULL) {
                ecsvm_set_error(error_message, error_message_capacity, "out of memory while resolving attributes");
                return 0;
            }

            free(semantic_struct->attributes[attribute_index]);
            semantic_struct->attributes[attribute_index] = resolved_attribute;
        }
    }

    for (struct_index = 0u; struct_index < semantic_functions->count; ++struct_index) {
        ecsvm_semantic_function_t *semantic_function;
        size_t parameter_index;
        char *resolved_return_type;

        semantic_function = &semantic_functions->items[struct_index];
        for (parameter_index = 0u; parameter_index < semantic_function->parameter_count; ++parameter_index) {
            char *resolved_type;

            resolved_type = ecsvm_resolve_type_name(
                semantic_structs,
                semantic_function->namespace_name,
                semantic_function->parameters[parameter_index].type_name
            );
            if (resolved_type == NULL) {
                ecsvm_set_error(error_message, error_message_capacity, "out of memory while resolving parameter types");
                return 0;
            }

            free(semantic_function->parameters[parameter_index].type_name);
            semantic_function->parameters[parameter_index].type_name = resolved_type;
            if (ecsvm_builtin_layout(resolved_type, NULL) == 0u &&
                strcmp(resolved_type, "core.Void") != 0 &&
                ecsvm_find_semantic_struct(semantic_structs, resolved_type) < 0) {
                ecsvm_set_error(error_message, error_message_capacity, "parameter type does not resolve");
                return 0;
            }
        }

        resolved_return_type = ecsvm_resolve_type_name(
            semantic_structs,
            semantic_function->namespace_name,
            semantic_function->return_type_name
        );
        if (resolved_return_type == NULL) {
            ecsvm_set_error(error_message, error_message_capacity, "out of memory while resolving function return types");
            return 0;
        }

        free(semantic_function->return_type_name);
        semantic_function->return_type_name = resolved_return_type;
        if (ecsvm_builtin_layout(resolved_return_type, NULL) == 0u &&
            strcmp(resolved_return_type, "core.Void") != 0 &&
            ecsvm_find_semantic_struct(semantic_structs, resolved_return_type) < 0) {
            ecsvm_set_error(error_message, error_message_capacity, "function return type does not resolve");
            return 0;
        }
    }

    return 1;
}

static int ecsvm_compute_struct_layout(
    ecsvm_semantic_struct_array_t *semantic_structs,
    size_t struct_index,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_semantic_struct_t *semantic_struct;
    size_t offset;
    size_t alignment;
    size_t field_index;

    semantic_struct = &semantic_structs->items[struct_index];
    if (semantic_struct->layout_state == 2) {
        return 1;
    }
    if (semantic_struct->layout_state == 1) {
        ecsvm_set_error(error_message, error_message_capacity, "recursive struct definitions are not supported");
        return 0;
    }

    semantic_struct->layout_state = 1;
    offset = 0u;
    alignment = 1u;
    for (field_index = 0u; field_index < semantic_struct->field_count; ++field_index) {
        ecsvm_semantic_field_t *field;
        size_t field_size;
        size_t field_alignment;
        int nested_index;

        field = &semantic_struct->fields[field_index];
        field_size = ecsvm_builtin_layout(field->type_name, &field_alignment);
        if (field_size == 0u) {
            nested_index = ecsvm_find_semantic_struct(semantic_structs, field->type_name);
            if (nested_index < 0 ||
                !ecsvm_compute_struct_layout(
                    semantic_structs,
                    (size_t)nested_index,
                    error_message,
                    error_message_capacity
                )) {
                return 0;
            }
            field_size = semantic_structs->items[nested_index].size;
            field_alignment = semantic_structs->items[nested_index].alignment;
        }

        offset = ecsvm_align_up(offset, field_alignment);
        offset += field_size;
        if (field_alignment > alignment) {
            alignment = field_alignment;
        }
    }

    semantic_struct->alignment = alignment;
    semantic_struct->size = ecsvm_align_up(offset, alignment);
    semantic_struct->layout_state = 2;
    return 1;
}

static int ecsvm_compute_layouts(
    ecsvm_semantic_struct_array_t *semantic_structs,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t index;

    for (index = 0u; index < semantic_structs->count; ++index) {
        if (!ecsvm_compute_struct_layout(
                semantic_structs,
                index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }
    }

    return 1;
}

static int ecsvm_find_blob(
    const ecsvm_blob_array_t *blobs,
    const void *data,
    size_t length
)
{
    size_t index;

    for (index = 0u; index < blobs->count; ++index) {
        if (blobs->items[index].length == length &&
            (length == 0u || memcmp(blobs->items[index].data, data, length) == 0)) {
            return (int)index;
        }
    }

    return -1;
}

static uint32_t ecsvm_ensure_blob(
    ecsvm_blob_array_t *blobs,
    const void *data,
    size_t length
)
{
    int existing;
    unsigned char *copy;
    ecsvm_blob_entry_t entry;

    existing = ecsvm_find_blob(blobs, data, length);
    if (existing >= 0) {
        return (uint32_t)existing + 1u;
    }

    copy = NULL;
    if (length > 0u) {
        copy = (unsigned char *)malloc(length);
        if (copy == NULL) {
            return 0u;
        }
        memcpy(copy, data, length);
    }

    entry.data = copy;
    entry.length = length;
    if (!ecsvm_blob_array_push(blobs, entry)) {
        free(copy);
        return 0u;
    }

    return (uint32_t)blobs->count;
}

static int ecsvm_split_qualified_name(
    const char *qualified_name,
    char **out_namespace,
    char **out_name
)
{
    const char *dot;

    dot = strrchr(qualified_name, '.');
    if (dot == NULL) {
        *out_namespace = ecsvm_copy_string("");
        *out_name = ecsvm_copy_string(qualified_name);
    } else {
        *out_namespace = ecsvm_copy_string_range(qualified_name, (size_t)(dot - qualified_name));
        *out_name = ecsvm_copy_string(dot + 1);
    }

    return *out_namespace != NULL && *out_name != NULL;
}

static int ecsvm_find_type_ref(
    const ecsvm_type_ref_builder_array_t *type_refs,
    const char *qualified_name
)
{
    size_t index;

    for (index = 0u; index < type_refs->count; ++index) {
        if (strcmp(type_refs->items[index].qualified_name, qualified_name) == 0) {
            return (int)index;
        }
    }

    return -1;
}

static uint32_t ecsvm_ensure_type_ref(
    ecsvm_type_ref_builder_array_t *type_refs,
    ecsvm_blob_array_t *blobs,
    const char *qualified_name
)
{
    int existing;
    ecsvm_type_ref_builder_t type_ref;

    existing = ecsvm_find_type_ref(type_refs, qualified_name);
    if (existing >= 0) {
        return (uint32_t)existing + 1u;
    }

    memset(&type_ref, 0, sizeof(type_ref));
    if (!ecsvm_split_qualified_name(
            qualified_name,
            &type_ref.namespace_name,
            &type_ref.name
        )) {
        free(type_ref.namespace_name);
        free(type_ref.name);
        return 0u;
    }

    type_ref.qualified_name = ecsvm_copy_string(qualified_name);
    if (type_ref.qualified_name == NULL) {
        free(type_ref.namespace_name);
        free(type_ref.name);
        return 0u;
    }

    type_ref.namespace_blob_id = ecsvm_ensure_blob(
        blobs,
        type_ref.namespace_name,
        strlen(type_ref.namespace_name)
    );
    type_ref.name_blob_id = ecsvm_ensure_blob(blobs, type_ref.name, strlen(type_ref.name));
    if (type_ref.namespace_blob_id == 0u || type_ref.name_blob_id == 0u ||
        !ecsvm_type_ref_builder_array_push(type_refs, type_ref)) {
        free(type_ref.namespace_name);
        free(type_ref.name);
        free(type_ref.qualified_name);
        return 0u;
    }

    return (uint32_t)type_refs->count;
}

static int ecsvm_build_ecsbin_tables(
    const ecsvm_semantic_struct_array_t *semantic_structs,
    const ecsvm_semantic_function_array_t *semantic_functions,
    ecsvm_blob_array_t *blobs,
    ecsvm_type_ref_builder_array_t *type_refs,
    ecsvm_field_ref_builder_array_t *field_refs,
    ecsvm_field_def_builder_array_t *field_defs,
    ecsvm_function_ref_builder_array_t *function_refs,
    ecsvm_parameter_builder_array_t *parameters,
    ecsvm_attribute_builder_array_t *attributes,
    ecsvm_struct_def_builder_array_t *struct_defs
)
{
    size_t struct_index;
    uint32_t empty_blob_id;

    empty_blob_id = ecsvm_ensure_blob(blobs, "", 0u);
    if (empty_blob_id == 0u) {
        return 0;
    }

    for (struct_index = 0u; struct_index < semantic_structs->count; ++struct_index) {
        const ecsvm_semantic_struct_t *semantic_struct;
        ecsvm_struct_def_builder_t struct_def;
        size_t field_index;
        size_t attribute_index;

        semantic_struct = &semantic_structs->items[struct_index];
        memset(&struct_def, 0, sizeof(struct_def));
        struct_def.type_id = ecsvm_ensure_type_ref(type_refs, blobs, semantic_struct->qualified_name);
        struct_def.flags = semantic_struct->is_component ? ECSVM_ECSBIN_STRUCT_FLAG_COMPONENT : 0u;
        struct_def.field_start = semantic_struct->field_count == 0u ? 0u : (uint32_t)field_refs->count + 1u;
        struct_def.attribute_start = semantic_struct->attribute_count == 0u ? 0u : (uint32_t)attributes->count + 1u;
        struct_def.field_count = (uint32_t)semantic_struct->field_count;
        struct_def.attribute_count = (uint32_t)semantic_struct->attribute_count;
        if (struct_def.type_id == 0u) {
            return 0;
        }

        for (attribute_index = 0u; attribute_index < semantic_struct->attribute_count; ++attribute_index) {
            ecsvm_attribute_builder_t attribute;

            attribute.type_id = ecsvm_ensure_type_ref(
                type_refs,
                blobs,
                semantic_struct->attributes[attribute_index]
            );
            attribute.data_blob_id = empty_blob_id;
            if (attribute.type_id == 0u ||
                !ecsvm_attribute_builder_array_push(attributes, attribute)) {
                return 0;
            }
        }

        for (field_index = 0u; field_index < semantic_struct->field_count; ++field_index) {
            ecsvm_field_ref_builder_t field_ref;
            ecsvm_field_def_builder_t field_def;

            field_ref.name = ecsvm_copy_string(semantic_struct->fields[field_index].name);
            if (field_ref.name == NULL) {
                return 0;
            }

            field_ref.name_blob_id = ecsvm_ensure_blob(
                blobs,
                field_ref.name,
                strlen(field_ref.name)
            );
            field_ref.type_id = ecsvm_ensure_type_ref(
                type_refs,
                blobs,
                semantic_struct->fields[field_index].type_name
            );
            if (field_ref.name_blob_id == 0u ||
                field_ref.type_id == 0u ||
                !ecsvm_field_ref_builder_array_push(field_refs, field_ref)) {
                free(field_ref.name);
                return 0;
            }

            field_def.field_id = (uint32_t)field_refs->count;
            field_def.attribute_start = 0u;
            field_def.attribute_count = 0u;
            if (!ecsvm_field_def_builder_array_push(field_defs, field_def)) {
                return 0;
            }
        }

        if (!ecsvm_struct_def_builder_array_push(struct_defs, struct_def)) {
            return 0;
        }
    }

    for (struct_index = 0u; struct_index < semantic_functions->count; ++struct_index) {
        const ecsvm_semantic_function_t *semantic_function;
        ecsvm_function_ref_builder_t function_ref;
        size_t parameter_index;

        semantic_function = &semantic_functions->items[struct_index];
        memset(&function_ref, 0, sizeof(function_ref));
        function_ref.namespace_name = ecsvm_copy_string(semantic_function->namespace_name);
        function_ref.name = ecsvm_copy_string(semantic_function->name);
        if (function_ref.namespace_name == NULL || function_ref.name == NULL) {
            free(function_ref.namespace_name);
            free(function_ref.name);
            return 0;
        }

        function_ref.namespace_blob_id = ecsvm_ensure_blob(
            blobs,
            function_ref.namespace_name,
            strlen(function_ref.namespace_name)
        );
        function_ref.name_blob_id = ecsvm_ensure_blob(
            blobs,
            function_ref.name,
            strlen(function_ref.name)
        );
        function_ref.parameter_start = semantic_function->parameter_count == 0u ? 0u : (uint32_t)parameters->count + 1u;
        function_ref.parameter_count = (uint32_t)semantic_function->parameter_count;
        function_ref.attribute_start = (uint32_t)attributes->count + 1u;
        function_ref.attribute_count = 1u + (uint32_t)semantic_function->attribute_count;
        function_ref.body_blob_id = semantic_function->body_ast_length == 0u
            ? 0u
            : ecsvm_ensure_blob(blobs, semantic_function->body_ast, semantic_function->body_ast_length);
        if (function_ref.namespace_blob_id == 0u ||
            function_ref.name_blob_id == 0u ||
            (semantic_function->body_ast_length > 0u && function_ref.body_blob_id == 0u)) {
            free(function_ref.namespace_name);
            free(function_ref.name);
            return 0;
        }

        {
            ecsvm_attribute_builder_t return_attribute;

            return_attribute.type_id = ecsvm_ensure_type_ref(
                type_refs,
                blobs,
                semantic_function->return_type_name
            );
            return_attribute.data_blob_id = empty_blob_id;
            if (return_attribute.type_id == 0u ||
                !ecsvm_attribute_builder_array_push(attributes, return_attribute)) {
                free(function_ref.namespace_name);
                free(function_ref.name);
                return 0;
            }
        }

        for (parameter_index = 0u; parameter_index < semantic_function->parameter_count; ++parameter_index) {
            const ecsvm_semantic_parameter_t *semantic_parameter;
            ecsvm_parameter_builder_t parameter;

            semantic_parameter = &semantic_function->parameters[parameter_index];
            memset(&parameter, 0, sizeof(parameter));
            parameter.name = ecsvm_copy_string(semantic_parameter->name);
            if (parameter.name == NULL) {
                free(function_ref.namespace_name);
                free(function_ref.name);
                return 0;
            }

            parameter.name_blob_id = ecsvm_ensure_blob(blobs, parameter.name, strlen(parameter.name));
            parameter.type_id = ecsvm_ensure_type_ref(type_refs, blobs, semantic_parameter->type_name);
            parameter.attribute_start = 0u;
            parameter.attribute_count = 0u;
            parameter.default_value_blob_id = semantic_parameter->default_value == NULL
                ? 0u
                : ecsvm_ensure_blob(
                    blobs,
                    semantic_parameter->default_value,
                    strlen(semantic_parameter->default_value)
                );
            if (parameter.name_blob_id == 0u ||
                parameter.type_id == 0u ||
                (semantic_parameter->default_value != NULL && parameter.default_value_blob_id == 0u) ||
                !ecsvm_parameter_builder_array_push(parameters, parameter)) {
                free(parameter.name);
                free(function_ref.namespace_name);
                free(function_ref.name);
                return 0;
            }
        }

        if (!ecsvm_function_ref_builder_array_push(function_refs, function_ref)) {
            free(function_ref.namespace_name);
            free(function_ref.name);
            return 0;
        }
    }

    return 1;
}

static int ecsvm_write_ecsbin_file(
    const char *path,
    const ecsvm_blob_array_t *blobs,
    const ecsvm_type_ref_builder_array_t *type_refs,
    const ecsvm_field_ref_builder_array_t *field_refs,
    const ecsvm_field_def_builder_array_t *field_defs,
    const ecsvm_function_ref_builder_array_t *function_refs,
    const ecsvm_parameter_builder_array_t *parameters,
    const ecsvm_attribute_builder_array_t *attributes,
    const ecsvm_struct_def_builder_array_t *struct_defs
)
{
    FILE *file;
    ecsvm_ecsbin_header_t header;
    uint64_t offset;
    size_t index;

    memset(&header, 0, sizeof(header));
    memcpy(header.magic, "ECSVM", 5u);
    header.version[0] = 0u;
    header.version[1] = 0u;
    header.version[2] = 2u;
    header.type_reference_count = (uint32_t)type_refs->count;
    header.field_reference_count = (uint32_t)field_refs->count;
    header.struct_definition_count = (uint32_t)struct_defs->count;
    header.field_definition_count = (uint32_t)field_defs->count;
    header.function_reference_count = (uint32_t)function_refs->count;
    header.parameter_count = (uint32_t)parameters->count;
    header.attribute_count = (uint32_t)attributes->count;
    header.blob_count = (uint32_t)blobs->count;

    offset = sizeof(header);
    header.type_reference_offset = offset;
    offset += type_refs->count * sizeof(ecsvm_type_ref_disk_t);
    header.field_reference_offset = offset;
    offset += field_refs->count * sizeof(ecsvm_field_ref_disk_t);
    header.struct_definition_offset = offset;
    offset += struct_defs->count * sizeof(ecsvm_struct_def_disk_t);
    header.field_definition_offset = offset;
    offset += field_defs->count * sizeof(ecsvm_field_def_disk_t);
    header.function_reference_offset = offset;
    offset += function_refs->count * sizeof(ecsvm_function_ref_disk_t);
    header.parameter_offset = offset;
    offset += parameters->count * sizeof(ecsvm_parameter_disk_t);
    header.attribute_offset = offset;
    offset += attributes->count * sizeof(ecsvm_attribute_disk_t);
    header.blob_offset = offset;

    file = fopen(path, "wb");
    if (file == NULL) {
        return 0;
    }

    if (fwrite(&header, 1u, sizeof(header), file) != sizeof(header)) {
        fclose(file);
        return 0;
    }

    for (index = 0u; index < type_refs->count; ++index) {
        ecsvm_type_ref_disk_t disk;

        disk.namespace_blob_id = type_refs->items[index].namespace_blob_id;
        disk.name_blob_id = type_refs->items[index].name_blob_id;
        if (fwrite(&disk, 1u, sizeof(disk), file) != sizeof(disk)) {
            fclose(file);
            return 0;
        }
    }

    for (index = 0u; index < field_refs->count; ++index) {
        ecsvm_field_ref_disk_t disk;

        disk.name_blob_id = field_refs->items[index].name_blob_id;
        disk.type_id = field_refs->items[index].type_id;
        if (fwrite(&disk, 1u, sizeof(disk), file) != sizeof(disk)) {
            fclose(file);
            return 0;
        }
    }

    for (index = 0u; index < struct_defs->count; ++index) {
        ecsvm_struct_def_disk_t disk;

        disk.type_id = struct_defs->items[index].type_id;
        disk.flags = struct_defs->items[index].flags;
        disk.field_start = struct_defs->items[index].field_start;
        disk.field_count = struct_defs->items[index].field_count;
        disk.attribute_start = struct_defs->items[index].attribute_start;
        disk.attribute_count = struct_defs->items[index].attribute_count;
        if (fwrite(&disk, 1u, sizeof(disk), file) != sizeof(disk)) {
            fclose(file);
            return 0;
        }
    }

    for (index = 0u; index < field_defs->count; ++index) {
        ecsvm_field_def_disk_t disk;

        disk.field_id = field_defs->items[index].field_id;
        disk.attribute_start = field_defs->items[index].attribute_start;
        disk.attribute_count = field_defs->items[index].attribute_count;
        if (fwrite(&disk, 1u, sizeof(disk), file) != sizeof(disk)) {
            fclose(file);
            return 0;
        }
    }

    for (index = 0u; index < function_refs->count; ++index) {
        ecsvm_function_ref_disk_t disk;

        disk.namespace_blob_id = function_refs->items[index].namespace_blob_id;
        disk.name_blob_id = function_refs->items[index].name_blob_id;
        disk.parameter_start = function_refs->items[index].parameter_start;
        disk.parameter_count = function_refs->items[index].parameter_count;
        disk.attribute_start = function_refs->items[index].attribute_start;
        disk.attribute_count = function_refs->items[index].attribute_count;
        disk.body_blob_id = function_refs->items[index].body_blob_id;
        if (fwrite(&disk, 1u, sizeof(disk), file) != sizeof(disk)) {
            fclose(file);
            return 0;
        }
    }

    for (index = 0u; index < parameters->count; ++index) {
        ecsvm_parameter_disk_t disk;

        disk.name_blob_id = parameters->items[index].name_blob_id;
        disk.type_id = parameters->items[index].type_id;
        disk.attribute_start = parameters->items[index].attribute_start;
        disk.attribute_count = parameters->items[index].attribute_count;
        disk.default_value_blob_id = parameters->items[index].default_value_blob_id;
        if (fwrite(&disk, 1u, sizeof(disk), file) != sizeof(disk)) {
            fclose(file);
            return 0;
        }
    }

    for (index = 0u; index < attributes->count; ++index) {
        ecsvm_attribute_disk_t disk;

        disk.type_id = attributes->items[index].type_id;
        disk.data_blob_id = attributes->items[index].data_blob_id;
        if (fwrite(&disk, 1u, sizeof(disk), file) != sizeof(disk)) {
            fclose(file);
            return 0;
        }
    }

    offset = 0u;
    for (index = 0u; index < blobs->count; ++index) {
        ecsvm_blob_disk_t disk;

        disk.offset = offset;
        disk.length = blobs->items[index].length;
        offset += blobs->items[index].length;
        if (fwrite(&disk, 1u, sizeof(disk), file) != sizeof(disk)) {
            fclose(file);
            return 0;
        }
    }

    for (index = 0u; index < blobs->count; ++index) {
        if (blobs->items[index].length > 0u &&
            fwrite(blobs->items[index].data, 1u, blobs->items[index].length, file) !=
                blobs->items[index].length) {
            fclose(file);
            return 0;
        }
    }

    fclose(file);
    return 1;
}

static const char *ecsvm_c_type_name(const char *qualified_name)
{
    if (strcmp(qualified_name, "core.Entity") == 0) {
        return "ecsvm_entity_t";
    }
    if (strcmp(qualified_name, "core.Int32") == 0) {
        return "int32_t";
    }
    if (strcmp(qualified_name, "core.UInt32") == 0) {
        return "uint32_t";
    }
    if (strcmp(qualified_name, "core.Float32") == 0) {
        return "float";
    }
    if (strcmp(qualified_name, "core.Blob") == 0 ||
        strcmp(qualified_name, "core.String") == 0) {
        return "ecsvm_blob_t";
    }
    if (strcmp(qualified_name, "core.Bool") == 0) {
        return "unsigned char";
    }
    return NULL;
}

static int ecsvm_write_c_identifier(FILE *file, const char *qualified_name)
{
    const char *cursor;

    for (cursor = qualified_name; *cursor != '\0'; ++cursor) {
        if (*cursor == '.') {
            if (fputc('_', file) == EOF) {
                return 0;
            }
        } else {
            if (fputc(*cursor, file) == EOF) {
                return 0;
            }
        }
    }
    return 1;
}

static int ecsvm_write_types_for_struct(
    FILE *file,
    ecsvm_semantic_struct_array_t *semantic_structs,
    size_t struct_index
)
{
    ecsvm_semantic_struct_t *semantic_struct;
    size_t field_index;

    semantic_struct = &semantic_structs->items[struct_index];
    if (semantic_struct->emit_state == 2) {
        return 1;
    }
    if (semantic_struct->emit_state == 1) {
        return 0;
    }

    semantic_struct->emit_state = 1;
    for (field_index = 0u; field_index < semantic_struct->field_count; ++field_index) {
        int nested_index;

        if (ecsvm_c_type_name(semantic_struct->fields[field_index].type_name) != NULL) {
            continue;
        }

        nested_index = ecsvm_find_semantic_struct(
            semantic_structs,
            semantic_struct->fields[field_index].type_name
        );
        if (nested_index < 0 ||
            !ecsvm_write_types_for_struct(file, semantic_structs, (size_t)nested_index)) {
            return 0;
        }
    }

    if (fprintf(file, "typedef struct ") < 0 ||
        !ecsvm_write_c_identifier(file, semantic_struct->qualified_name) ||
        fprintf(file, " {\n") < 0) {
        return 0;
    }

    for (field_index = 0u; field_index < semantic_struct->field_count; ++field_index) {
        const char *c_type;

        c_type = ecsvm_c_type_name(semantic_struct->fields[field_index].type_name);
        if (c_type != NULL) {
            if (fprintf(
                    file,
                    "    %s %s;\n",
                    c_type,
                    semantic_struct->fields[field_index].name
                ) < 0) {
                return 0;
            }
        } else {
            int nested_index;

            nested_index = ecsvm_find_semantic_struct(
                semantic_structs,
                semantic_struct->fields[field_index].type_name
            );
            if (nested_index < 0 ||
                fprintf(file, "    ") < 0 ||
                !ecsvm_write_c_identifier(file, semantic_structs->items[nested_index].qualified_name) ||
                fprintf(file, "_t %s;\n", semantic_struct->fields[field_index].name) < 0) {
                return 0;
            }
        }
    }

    if (fprintf(file, "} ") < 0 ||
        !ecsvm_write_c_identifier(file, semantic_struct->qualified_name) ||
        fprintf(file, "_t;\n\n") < 0) {
        return 0;
    }

    semantic_struct->emit_state = 2;
    return 1;
}

static int ecsvm_write_types_header(
    const char *path,
    const ecsvm_manifest_t *manifest,
    ecsvm_semantic_struct_array_t *semantic_structs
)
{
    FILE *file;
    char guard[128];
    size_t index;
    size_t guard_index;

    file = fopen(path, "wb");
    if (file == NULL) {
        return 0;
    }

    guard_index = 0u;
    for (index = 0u; manifest->name[index] != '\0' && guard_index + 16u < sizeof(guard); ++index) {
        char ch;

        ch = manifest->name[index];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
            if (ch >= 'a' && ch <= 'z') {
                ch = (char)(ch - ('a' - 'A'));
            }
            guard[guard_index] = ch;
        } else {
            guard[guard_index] = '_';
        }
        guard_index += 1u;
    }
    memcpy(guard + guard_index, "_TYPES_H", 9u);

    if (fprintf(
            file,
            "#ifndef %s\n#define %s\n\n#include \"ecsvm/ecsvm.h\"\n\n#include <stdint.h>\n\n",
            guard,
            guard
        ) < 0) {
        fclose(file);
        return 0;
    }

    for (index = 0u; index < semantic_structs->count; ++index) {
        if (!ecsvm_write_types_for_struct(file, semantic_structs, index)) {
            fclose(file);
            return 0;
        }
    }

    if (fprintf(file, "#endif\n") < 0) {
        fclose(file);
        return 0;
    }

    fclose(file);
    return 1;
}

static int ecsvm_ensure_directory(const char *path)
{
#ifdef _WIN32
    if (_mkdir(path) == 0 || errno == EEXIST) {
#else
    if (mkdir(path, 0777) == 0 || errno == EEXIST) {
#endif
        return 1;
    }

    return 0;
}

ecsvm_status_t ecsvm_project_build(
    const char *project_path,
    char *out_ecsbin_path,
    size_t out_ecsbin_path_capacity,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_manifest_t manifest;
    ecsvm_string_array_t source_paths;
    ecsvm_source_file_array_t files;
    ecsvm_semantic_struct_array_t semantic_structs;
    ecsvm_semantic_function_array_t semantic_functions;
    ecsvm_blob_array_t blobs;
    ecsvm_type_ref_builder_array_t type_refs;
    ecsvm_field_ref_builder_array_t field_refs;
    ecsvm_field_def_builder_array_t field_defs;
    ecsvm_function_ref_builder_array_t function_refs;
    ecsvm_parameter_builder_array_t parameters;
    ecsvm_attribute_builder_array_t attributes;
    ecsvm_struct_def_builder_array_t struct_defs;
    char src_path[MAX_PATH];
    char entry_path[MAX_PATH];
    char out_path[MAX_PATH];
    char ecsbin_name[MAX_PATH];
    char ecsbin_path[MAX_PATH];
    char types_path[MAX_PATH];
    size_t source_index;
    ecsvm_status_t result;

    memset(&manifest, 0, sizeof(manifest));
    memset(&source_paths, 0, sizeof(source_paths));
    memset(&files, 0, sizeof(files));
    memset(&semantic_structs, 0, sizeof(semantic_structs));
    memset(&semantic_functions, 0, sizeof(semantic_functions));
    memset(&blobs, 0, sizeof(blobs));
    memset(&type_refs, 0, sizeof(type_refs));
    memset(&field_refs, 0, sizeof(field_refs));
    memset(&field_defs, 0, sizeof(field_defs));
    memset(&function_refs, 0, sizeof(function_refs));
    memset(&parameters, 0, sizeof(parameters));
    memset(&attributes, 0, sizeof(attributes));
    memset(&struct_defs, 0, sizeof(struct_defs));
    ecsvm_set_error(error_message, error_message_capacity, NULL);

    if (project_path == NULL || !ecsvm_path_is_directory(project_path)) {
        ecsvm_set_error(error_message, error_message_capacity, "project path must be a directory");
        return ECSVM_ERROR_ARGUMENT;
    }

    if (!ecsvm_parse_manifest(project_path, &manifest, error_message, error_message_capacity)) {
        return ECSVM_ERROR_ARGUMENT;
    }

    if (!ecsvm_path_join(project_path, manifest.entry, entry_path, sizeof(entry_path)) ||
        !ecsvm_path_exists(entry_path)) {
        ecsvm_manifest_free(&manifest);
        ecsvm_set_error(error_message, error_message_capacity, "manifest entry file does not exist");
        return ECSVM_ERROR_ARGUMENT;
    }

    if (!ecsvm_path_join(project_path, "src", src_path, sizeof(src_path)) ||
        !ecsvm_path_is_directory(src_path)) {
        ecsvm_manifest_free(&manifest);
        ecsvm_set_error(error_message, error_message_capacity, "project src directory does not exist");
        return ECSVM_ERROR_ARGUMENT;
    }

    if (!ecsvm_collect_ecs_files_recursive(
            src_path,
            &source_paths,
            error_message,
            error_message_capacity
        )) {
        ecsvm_manifest_free(&manifest);
        ecsvm_string_array_free(&source_paths);
        return ECSVM_ERROR_ARGUMENT;
    }

    if (source_paths.count == 0u) {
        ecsvm_manifest_free(&manifest);
        ecsvm_string_array_free(&source_paths);
        ecsvm_set_error(error_message, error_message_capacity, "project does not contain any .ecs files");
        return ECSVM_ERROR_ARGUMENT;
    }

    qsort(source_paths.items, source_paths.count, sizeof(*source_paths.items), ecsvm_compare_strings);
    for (source_index = 0u; source_index < source_paths.count; ++source_index) {
        ecsvm_source_file_t file;

        memset(&file, 0, sizeof(file));
        file.path = source_paths.items[source_index];
        source_paths.items[source_index] = NULL;
        if (!ecsvm_read_text_file(file.path, &file.source, &file.length) ||
            !ecsvm_lex_source(&file, error_message, error_message_capacity) ||
            !ecsvm_parse_file(&file, error_message, error_message_capacity) ||
            !ecsvm_source_file_array_push(&files, file)) {
            ecsvm_source_file_free(&file);
            result = ECSVM_ERROR_ARGUMENT;
            goto cleanup;
        }
    }
    ecsvm_string_array_free(&source_paths);

    if (!ecsvm_collect_semantics(&files, &semantic_structs, &semantic_functions, error_message, error_message_capacity) ||
        !ecsvm_resolve_semantic_types(&semantic_structs, &semantic_functions, error_message, error_message_capacity) ||
        !ecsvm_compute_layouts(&semantic_structs, error_message, error_message_capacity)) {
        result = ECSVM_ERROR_ARGUMENT;
        goto cleanup;
    }

    if (!ecsvm_path_join(project_path, "out", out_path, sizeof(out_path)) ||
        !ecsvm_ensure_directory(out_path)) {
        ecsvm_set_error(error_message, error_message_capacity, "failed to create out directory");
        result = ECSVM_ERROR_NOT_FOUND;
        goto cleanup;
    }

    if (!ecsvm_path_join(out_path, "types.h", types_path, sizeof(types_path)) ||
        snprintf(ecsbin_name, sizeof(ecsbin_name), "%s.ecsbin", manifest.name) <= 0 ||
        !ecsvm_path_join(out_path, ecsbin_name, ecsbin_path, sizeof(ecsbin_path))) {
        ecsvm_set_error(error_message, error_message_capacity, "output path is too long");
        result = ECSVM_ERROR_ARGUMENT;
        goto cleanup;
    }

    if (!ecsvm_build_ecsbin_tables(
            &semantic_structs,
            &semantic_functions,
            &blobs,
            &type_refs,
            &field_refs,
            &field_defs,
            &function_refs,
            &parameters,
            &attributes,
            &struct_defs
        ) ||
        !ecsvm_write_ecsbin_file(
            ecsbin_path,
            &blobs,
            &type_refs,
            &field_refs,
            &field_defs,
            &function_refs,
            &parameters,
            &attributes,
            &struct_defs
        ) ||
        !ecsvm_write_types_header(types_path, &manifest, &semantic_structs)) {
        ecsvm_set_error(error_message, error_message_capacity, "failed to write project output");
        result = ECSVM_ERROR_NOT_FOUND;
        goto cleanup;
    }

    if (out_ecsbin_path != NULL && out_ecsbin_path_capacity > 0u) {
        (void)snprintf(out_ecsbin_path, out_ecsbin_path_capacity, "%s", ecsbin_path);
    }

    result = ECSVM_OK;

cleanup:
    ecsvm_manifest_free(&manifest);
    ecsvm_string_array_free(&source_paths);
    ecsvm_source_file_array_free(&files);
    ecsvm_semantic_struct_array_free(&semantic_structs);
    ecsvm_semantic_function_array_free(&semantic_functions);
    ecsvm_blob_array_free(&blobs);
    ecsvm_type_ref_builder_array_free(&type_refs);
    ecsvm_field_ref_builder_array_free(&field_refs);
    ecsvm_field_def_builder_array_free(&field_defs);
    ecsvm_function_ref_builder_array_free(&function_refs);
    ecsvm_parameter_builder_array_free(&parameters);
    ecsvm_attribute_builder_array_free(&attributes);
    ecsvm_struct_def_builder_array_free(&struct_defs);
    return result;
}
