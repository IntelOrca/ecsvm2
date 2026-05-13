#include "ecsvm/ecsbin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
typedef __int64 ecsvm_file_offset_t;
#define ECSVM_FTELL _ftelli64
#define ECSVM_FSEEK _fseeki64
#else
typedef long ecsvm_file_offset_t;
#define ECSVM_FTELL ftell
#define ECSVM_FSEEK fseek
#endif

#define ECSVM_ALIGNOF(type) offsetof(struct { char pad; type value; }, value)

typedef struct ecsvm_ecsbin_header_prefix {
    char magic[5];
    unsigned char version[3];
} ecsvm_ecsbin_header_prefix_t;

typedef struct ecsvm_ecsbin_header_v1 {
    char magic[5];
    unsigned char version[3];
    uint64_t type_reference_offset;
    uint64_t field_reference_offset;
    uint64_t struct_definition_offset;
    uint64_t field_definition_offset;
    uint64_t attribute_offset;
    uint64_t blob_offset;
    uint32_t type_reference_count;
    uint32_t field_reference_count;
    uint32_t struct_definition_count;
    uint32_t field_definition_count;
    uint32_t attribute_count;
    uint32_t blob_count;
} ecsvm_ecsbin_header_v1_t;

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

typedef struct ecsvm_ecsbin_type_ref_disk {
    uint32_t namespace_blob_id;
    uint32_t name_blob_id;
} ecsvm_ecsbin_type_ref_disk_t;

typedef struct ecsvm_ecsbin_field_ref_disk {
    uint32_t name_blob_id;
    uint32_t type_id;
} ecsvm_ecsbin_field_ref_disk_t;

typedef struct ecsvm_ecsbin_struct_def_disk {
    uint32_t type_id;
    uint32_t flags;
    uint32_t field_start;
    uint32_t field_count;
    uint32_t attribute_start;
    uint32_t attribute_count;
} ecsvm_ecsbin_struct_def_disk_t;

typedef struct ecsvm_ecsbin_field_def_disk {
    uint32_t field_id;
    uint32_t attribute_start;
    uint32_t attribute_count;
} ecsvm_ecsbin_field_def_disk_t;

typedef struct ecsvm_ecsbin_function_ref_disk {
    uint32_t namespace_blob_id;
    uint32_t name_blob_id;
    uint32_t parameter_start;
    uint32_t parameter_count;
    uint32_t attribute_start;
    uint32_t attribute_count;
    uint32_t body_blob_id;
} ecsvm_ecsbin_function_ref_disk_t;

typedef struct ecsvm_ecsbin_parameter_disk {
    uint32_t name_blob_id;
    uint32_t type_id;
    uint32_t attribute_start;
    uint32_t attribute_count;
    uint32_t default_value_blob_id;
} ecsvm_ecsbin_parameter_disk_t;

typedef struct ecsvm_ecsbin_attribute_disk {
    uint32_t type_id;
    uint32_t data_blob_id;
} ecsvm_ecsbin_attribute_disk_t;

typedef struct ecsvm_ecsbin_blob_disk {
    uint64_t offset;
    uint64_t length;
} ecsvm_ecsbin_blob_disk_t;

typedef enum ecsvm_ecsbin_token_kind {
    ECSVM_ECSBIN_TOKEN_EOF = 0,
    ECSVM_ECSBIN_TOKEN_IDENTIFIER,
    ECSVM_ECSBIN_TOKEN_NUMBER,
    ECSVM_ECSBIN_TOKEN_STRING,
    ECSVM_ECSBIN_TOKEN_LBRACE,
    ECSVM_ECSBIN_TOKEN_RBRACE,
    ECSVM_ECSBIN_TOKEN_LBRACKET,
    ECSVM_ECSBIN_TOKEN_RBRACKET,
    ECSVM_ECSBIN_TOKEN_LPAREN,
    ECSVM_ECSBIN_TOKEN_RPAREN,
    ECSVM_ECSBIN_TOKEN_COLON,
    ECSVM_ECSBIN_TOKEN_SEMICOLON,
    ECSVM_ECSBIN_TOKEN_DOT,
    ECSVM_ECSBIN_TOKEN_COMMA,
    ECSVM_ECSBIN_TOKEN_EQUAL,
    ECSVM_ECSBIN_TOKEN_BANG,
    ECSVM_ECSBIN_TOKEN_PLUS,
    ECSVM_ECSBIN_TOKEN_MINUS,
    ECSVM_ECSBIN_TOKEN_STAR,
    ECSVM_ECSBIN_TOKEN_SLASH,
    ECSVM_ECSBIN_TOKEN_PERCENT,
    ECSVM_ECSBIN_TOKEN_LT,
    ECSVM_ECSBIN_TOKEN_GT,
    ECSVM_ECSBIN_TOKEN_AMPERSAND,
    ECSVM_ECSBIN_TOKEN_PIPE,
    ECSVM_ECSBIN_TOKEN_CARET,
    ECSVM_ECSBIN_TOKEN_TILDE,
    ECSVM_ECSBIN_TOKEN_KEY_IMPORT,
    ECSVM_ECSBIN_TOKEN_KEY_NAMESPACE,
    ECSVM_ECSBIN_TOKEN_KEY_STRUCT,
    ECSVM_ECSBIN_TOKEN_KEY_COMPONENT,
    ECSVM_ECSBIN_TOKEN_KEY_SYSTEM,
    ECSVM_ECSBIN_TOKEN_KEY_CONST,
    ECSVM_ECSBIN_TOKEN_KEY_FN
} ecsvm_ecsbin_token_kind_t;

typedef enum ecsvm_ecsbin_ast_node_kind {
    ECSVM_ECSBIN_AST_NODE_ROOT = 1,
    ECSVM_ECSBIN_AST_NODE_BLOCK,
    ECSVM_ECSBIN_AST_NODE_GROUP_PAREN,
    ECSVM_ECSBIN_AST_NODE_GROUP_BRACKET,
    ECSVM_ECSBIN_AST_NODE_TOKEN
} ecsvm_ecsbin_ast_node_kind_t;

typedef struct ecsvm_ecsbin_ast_node {
    uint32_t kind;
    uint32_t first_child;
    uint32_t last_child;
    uint32_t next_sibling;
    uint32_t token_kind;
    uint32_t text_offset;
    uint32_t text_length;
} ecsvm_ecsbin_ast_node_t;

typedef struct ecsvm_ecsbin_ast_blob {
    const ecsvm_ecsbin_ast_node_t *nodes;
    size_t node_count;
    const unsigned char *text_data;
    size_t text_length;
} ecsvm_ecsbin_ast_blob_t;

typedef struct ecsvm_ecsbin_text_buffer {
    char *data;
    size_t length;
    size_t capacity;
} ecsvm_ecsbin_text_buffer_t;

enum {
    ECSVM_ECSBIN_VERSION_0 = 0u,
    ECSVM_ECSBIN_VERSION_1 = 0u,
    ECSVM_ECSBIN_VERSION_2_V1 = 1u,
    ECSVM_ECSBIN_VERSION_2 = 2u,
    ECSVM_ECSBIN_AST_VERSION_1 = 1u
};

_Static_assert(sizeof(ecsvm_ecsbin_header_v1_t) == 80u, "ecsbin v1 header size");
_Static_assert(sizeof(ecsvm_ecsbin_header_t) == 104u, "ecsbin header size");
_Static_assert(sizeof(ecsvm_ecsbin_blob_disk_t) == 16u, "ecsbin blob size");

static void ecsvm_ecsbin_set_error(
    char *error_message,
    size_t error_message_capacity,
    const char *message
)
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

static char *ecsvm_ecsbin_copy_string_range(const unsigned char *data, size_t length)
{
    char *copy;

    copy = (char *)malloc(length + 1u);
    if (copy == NULL) {
        return NULL;
    }

    if (length > 0u) {
        memcpy(copy, data, length);
    }
    copy[length] = '\0';
    return copy;
}

static uint64_t ecsvm_ecsbin_file_size(FILE *file)
{
    ecsvm_file_offset_t current;
    ecsvm_file_offset_t end;

    current = ECSVM_FTELL(file);
    if (current < 0) {
        return 0u;
    }

    if (ECSVM_FSEEK(file, 0, SEEK_END) != 0) {
        return 0u;
    }

    end = ECSVM_FTELL(file);
    if (end < 0) {
        return 0u;
    }

    if (ECSVM_FSEEK(file, current, SEEK_SET) != 0) {
        return 0u;
    }

    return (uint64_t)end;
}

static int ecsvm_ecsbin_seek(FILE *file, uint64_t offset)
{
    return ECSVM_FSEEK(file, (ecsvm_file_offset_t)offset, SEEK_SET) == 0;
}

static int ecsvm_ecsbin_read_exact(FILE *file, void *data, size_t size)
{
    return size == 0u || fread(data, 1u, size, file) == size;
}

static int ecsvm_ecsbin_text_buffer_reserve(
    ecsvm_ecsbin_text_buffer_t *buffer,
    size_t additional
)
{
    size_t required;
    size_t capacity;
    char *data;

    if (buffer == NULL) {
        return 0;
    }

    required = buffer->length + additional + 1u;
    if (required <= buffer->capacity) {
        return 1;
    }

    capacity = buffer->capacity == 0u ? 128u : buffer->capacity;
    while (capacity < required) {
        capacity *= 2u;
    }

    data = (char *)realloc(buffer->data, capacity);
    if (data == NULL) {
        return 0;
    }

    buffer->data = data;
    buffer->capacity = capacity;
    return 1;
}

static int ecsvm_ecsbin_text_buffer_append_range(
    ecsvm_ecsbin_text_buffer_t *buffer,
    const char *text,
    size_t length
)
{
    if (!ecsvm_ecsbin_text_buffer_reserve(buffer, length)) {
        return 0;
    }

    if (length > 0u) {
        memcpy(buffer->data + buffer->length, text, length);
        buffer->length += length;
    }
    buffer->data[buffer->length] = '\0';
    return 1;
}

static int ecsvm_ecsbin_text_buffer_append(
    ecsvm_ecsbin_text_buffer_t *buffer,
    const char *text
)
{
    return text != NULL && ecsvm_ecsbin_text_buffer_append_range(buffer, text, strlen(text));
}

static int ecsvm_ecsbin_text_buffer_append_char(
    ecsvm_ecsbin_text_buffer_t *buffer,
    char ch
)
{
    if (!ecsvm_ecsbin_text_buffer_reserve(buffer, 1u)) {
        return 0;
    }

    buffer->data[buffer->length] = ch;
    buffer->length += 1u;
    buffer->data[buffer->length] = '\0';
    return 1;
}

static int ecsvm_ecsbin_text_buffer_append_indent(
    ecsvm_ecsbin_text_buffer_t *buffer,
    size_t indent
)
{
    size_t index;

    for (index = 0u; index < indent; ++index) {
        if (!ecsvm_ecsbin_text_buffer_append(buffer, "    ")) {
            return 0;
        }
    }
    return 1;
}

static char *ecsvm_ecsbin_blob_string(
    const ecsvm_ecsbin_module_t *module,
    uint32_t blob_id
)
{
    const ecsvm_ecsbin_blob_t *blob;

    if (module == NULL || blob_id == 0u || blob_id > module->blob_count) {
        return NULL;
    }

    blob = &module->blobs[blob_id - 1u];
    return ecsvm_ecsbin_copy_string_range(blob->data, (size_t)blob->length);
}

static char *ecsvm_ecsbin_compose_qualified_name(
    const char *namespace_name,
    const char *name
)
{
    size_t namespace_length;
    size_t name_length;
    char *qualified_name;

    if (name == NULL) {
        return NULL;
    }

    namespace_length = namespace_name != NULL ? strlen(namespace_name) : 0u;
    name_length = strlen(name);
    if (namespace_length == 0u) {
        return ecsvm_ecsbin_copy_string_range((const unsigned char *)name, name_length);
    }

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

static int ecsvm_ecsbin_range_is_valid(size_t total_count, uint32_t start, uint32_t count)
{
    if (count == 0u) {
        return start == 0u;
    }

    if (start == 0u) {
        return 0;
    }

    return (size_t)(start - 1u) + (size_t)count <= total_count;
}

static size_t ecsvm_ecsbin_builtin_layout(
    const char *qualified_name,
    size_t *out_alignment
)
{
    size_t size;
    size_t alignment;

    size = 0u;
    alignment = 0u;
    if (qualified_name == NULL) {
        if (out_alignment != NULL) {
            *out_alignment = 0u;
        }
        return 0u;
    }

    if (strcmp(qualified_name, "core.Entity") == 0) {
        size = sizeof(ecsvm_entity_t);
        alignment = ECSVM_ALIGNOF(ecsvm_entity_t);
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
        size = sizeof(ecsvm_blob_t);
        alignment = ECSVM_ALIGNOF(ecsvm_blob_t);
    } else if (strcmp(qualified_name, "core.Bool") == 0) {
        size = sizeof(unsigned char);
        alignment = ECSVM_ALIGNOF(unsigned char);
    }

    if (out_alignment != NULL) {
        *out_alignment = alignment;
    }
    return size;
}

static size_t ecsvm_ecsbin_align_up(size_t value, size_t alignment)
{
    if (alignment == 0u) {
        return value;
    }

    return (value + alignment - 1u) / alignment * alignment;
}

static int ecsvm_ecsbin_find_struct_index_by_type(
    const ecsvm_ecsbin_module_t *module,
    uint32_t type_id
)
{
    size_t index;

    if (module == NULL || type_id == 0u) {
        return -1;
    }

    for (index = 0u; index < module->struct_def_count; ++index) {
        if (module->struct_defs[index].type_id == type_id) {
            return (int)index;
        }
    }

    return -1;
}

static ecsvm_status_t ecsvm_ecsbin_compute_struct_layout(
    ecsvm_ecsbin_module_t *module,
    size_t struct_index,
    unsigned char *visit_state,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_ecsbin_struct_def_t *definition;
    size_t size;
    size_t alignment;
    size_t field_index;

    if (module == NULL || struct_index >= module->struct_def_count || visit_state == NULL) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "invalid struct layout request");
        return ECSVM_ERROR_ARGUMENT;
    }

    if (visit_state[struct_index] == 2u) {
        return ECSVM_OK;
    }

    if (visit_state[struct_index] == 1u) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "recursive struct definitions are not supported");
        return ECSVM_ERROR_ARGUMENT;
    }

    visit_state[struct_index] = 1u;
    definition = &module->struct_defs[struct_index];
    size = 0u;
    alignment = 1u;

    for (field_index = 0u; field_index < definition->field_count; ++field_index) {
        const ecsvm_ecsbin_field_ref_t *field_ref;
        const ecsvm_ecsbin_type_ref_t *type_ref;
        size_t field_size;
        size_t field_alignment;
        int nested_index;

        if (definition->field_start == 0u ||
            definition->field_start - 1u + field_index >= module->field_ref_count) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "field range is out of bounds");
            return ECSVM_ERROR_ARGUMENT;
        }

        field_ref = &module->field_refs[definition->field_start - 1u + field_index];
        type_ref = ecsvm_ecsbin_type_ref(module, field_ref->type_id);
        if (type_ref == NULL || type_ref->qualified_name == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "field type reference is invalid");
            return ECSVM_ERROR_ARGUMENT;
        }

        field_size = ecsvm_ecsbin_builtin_layout(type_ref->qualified_name, &field_alignment);
        if (field_size == 0u) {
            nested_index = ecsvm_ecsbin_find_struct_index_by_type(module, field_ref->type_id);
            if (nested_index < 0) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "field type does not resolve to a known struct");
                return ECSVM_ERROR_ARGUMENT;
            }

            if (ecsvm_ecsbin_compute_struct_layout(
                    module,
                    (size_t)nested_index,
                    visit_state,
                    error_message,
                    error_message_capacity
                ) != ECSVM_OK) {
                return ECSVM_ERROR_ARGUMENT;
            }

            field_size = module->struct_defs[nested_index].size;
            field_alignment = module->struct_defs[nested_index].alignment;
        }

        size = ecsvm_ecsbin_align_up(size, field_alignment);
        size += field_size;
        if (field_alignment > alignment) {
            alignment = field_alignment;
        }
    }

    definition->alignment = alignment;
    definition->size = ecsvm_ecsbin_align_up(size, alignment);
    visit_state[struct_index] = 2u;
    return ECSVM_OK;
}

const ecsvm_ecsbin_type_ref_t *ecsvm_ecsbin_type_ref(
    const ecsvm_ecsbin_module_t *module,
    uint32_t type_id
)
{
    if (module == NULL || type_id == 0u || type_id > module->type_ref_count) {
        return NULL;
    }

    return &module->type_refs[type_id - 1u];
}

const ecsvm_ecsbin_parameter_t *ecsvm_ecsbin_parameter_ref(
    const ecsvm_ecsbin_module_t *module,
    uint32_t parameter_id
)
{
    if (module == NULL || parameter_id == 0u || parameter_id > module->parameter_count) {
        return NULL;
    }

    return &module->parameters[parameter_id - 1u];
}

const ecsvm_ecsbin_attribute_t *ecsvm_ecsbin_attribute_ref(
    const ecsvm_ecsbin_module_t *module,
    uint32_t attribute_id
)
{
    if (module == NULL || attribute_id == 0u || attribute_id > module->attribute_count) {
        return NULL;
    }

    return &module->attributes[attribute_id - 1u];
}

const ecsvm_ecsbin_blob_t *ecsvm_ecsbin_blob_ref(
    const ecsvm_ecsbin_module_t *module,
    uint32_t blob_id
)
{
    if (module == NULL || blob_id == 0u || blob_id > module->blob_count) {
        return NULL;
    }

    return &module->blobs[blob_id - 1u];
}

const ecsvm_ecsbin_type_ref_t *ecsvm_ecsbin_function_return_type(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_function_ref_t *function_ref
)
{
    const ecsvm_ecsbin_attribute_t *attribute;

    if (module == NULL || function_ref == NULL || function_ref->attribute_count == 0u) {
        return NULL;
    }

    attribute = ecsvm_ecsbin_attribute_ref(module, function_ref->attribute_start);
    if (attribute == NULL) {
        return NULL;
    }

    return ecsvm_ecsbin_type_ref(module, attribute->type_id);
}

static const ecsvm_ecsbin_ast_node_t *ecsvm_ecsbin_ast_node(
    const ecsvm_ecsbin_ast_blob_t *ast,
    uint32_t index,
    char *error_message,
    size_t error_message_capacity
)
{
    if (ast == NULL || index >= ast->node_count) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function body references an invalid ast node");
        return NULL;
    }

    return &ast->nodes[index];
}

static int ecsvm_ecsbin_ast_child_link_is_valid(
    const ecsvm_ecsbin_ast_blob_t *ast,
    uint32_t index
)
{
    return index == 0u || index < ast->node_count;
}

static ecsvm_status_t ecsvm_ecsbin_parse_ast_blob(
    const ecsvm_ecsbin_blob_t *blob,
    ecsvm_ecsbin_ast_blob_t *out_ast,
    char *error_message,
    size_t error_message_capacity
)
{
    uint32_t version;
    uint32_t node_count;
    size_t node_bytes;
    size_t header_bytes;

    ecsvm_ecsbin_set_error(error_message, error_message_capacity, NULL);
    if (blob == NULL || out_ast == NULL) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function body blob is required");
        return ECSVM_ERROR_ARGUMENT;
    }

    if (blob->length < sizeof(uint32_t) * 2u) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function body ast header is truncated");
        return ECSVM_ERROR_ARGUMENT;
    }

    memcpy(&version, blob->data, sizeof(version));
    memcpy(&node_count, blob->data + sizeof(uint32_t), sizeof(node_count));
    if (version != ECSVM_ECSBIN_AST_VERSION_1) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function body ast version is not supported");
        return ECSVM_ERROR_ARGUMENT;
    }

    node_bytes = (size_t)node_count * sizeof(ecsvm_ecsbin_ast_node_t);
    header_bytes = sizeof(uint32_t) * 2u;
    if (node_count == 0u || header_bytes + node_bytes > blob->length) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function body ast is truncated");
        return ECSVM_ERROR_ARGUMENT;
    }

    out_ast->nodes = (const ecsvm_ecsbin_ast_node_t *)(const void *)(blob->data + header_bytes);
    out_ast->node_count = (size_t)node_count;
    out_ast->text_data = blob->data + header_bytes + node_bytes;
    out_ast->text_length = (size_t)blob->length - header_bytes - node_bytes;
    return ECSVM_OK;
}

static ecsvm_status_t ecsvm_ecsbin_render_ast_inline(
    const ecsvm_ecsbin_ast_blob_t *ast,
    uint32_t child_index,
    ecsvm_ecsbin_text_buffer_t *buffer,
    char *error_message,
    size_t error_message_capacity
);

static ecsvm_status_t ecsvm_ecsbin_render_ast_inline_node(
    const ecsvm_ecsbin_ast_blob_t *ast,
    uint32_t node_index,
    ecsvm_ecsbin_text_buffer_t *buffer,
    char *error_message,
    size_t error_message_capacity
);

static int ecsvm_ecsbin_node_needs_no_leading_space(
    const ecsvm_ecsbin_ast_node_t *node
)
{
    if (node == NULL) {
        return 0;
    }

    if (node->kind == ECSVM_ECSBIN_AST_NODE_GROUP_BRACKET) {
        return 1;
    }

    return node->kind == ECSVM_ECSBIN_AST_NODE_TOKEN &&
        (node->token_kind == ECSVM_ECSBIN_TOKEN_COMMA ||
         node->token_kind == ECSVM_ECSBIN_TOKEN_SEMICOLON ||
         node->token_kind == ECSVM_ECSBIN_TOKEN_DOT);
}

static int ecsvm_ecsbin_node_needs_no_trailing_space(
    const ecsvm_ecsbin_ast_node_t *node
)
{
    if (node == NULL || node->kind != ECSVM_ECSBIN_AST_NODE_TOKEN) {
        return 0;
    }

    return node->token_kind == ECSVM_ECSBIN_TOKEN_DOT ||
        node->token_kind == ECSVM_ECSBIN_TOKEN_BANG ||
        node->token_kind == ECSVM_ECSBIN_TOKEN_TILDE;
}

static ecsvm_status_t ecsvm_ecsbin_render_ast_block(
    const ecsvm_ecsbin_ast_blob_t *ast,
    uint32_t node_index,
    size_t indent,
    ecsvm_ecsbin_text_buffer_t *buffer,
    char *error_message,
    size_t error_message_capacity
)
{
    const ecsvm_ecsbin_ast_node_t *node;
    uint32_t child_index;

    node = ecsvm_ecsbin_ast_node(ast, node_index, error_message, error_message_capacity);
    if (node == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    if (node->kind != ECSVM_ECSBIN_AST_NODE_BLOCK &&
        node->kind != ECSVM_ECSBIN_AST_NODE_ROOT) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function body ast node is not a block");
        return ECSVM_ERROR_ARGUMENT;
    }

    if (!ecsvm_ecsbin_ast_child_link_is_valid(ast, node->first_child) ||
        !ecsvm_ecsbin_ast_child_link_is_valid(ast, node->last_child)) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function body ast child range is invalid");
        return ECSVM_ERROR_ARGUMENT;
    }

    if (node->kind == ECSVM_ECSBIN_AST_NODE_ROOT) {
        return ecsvm_ecsbin_render_ast_inline(
            ast,
            node->first_child,
            buffer,
            error_message,
            error_message_capacity
        );
    }

    if (node->first_child == 0u) {
        if (!ecsvm_ecsbin_text_buffer_append(buffer, "{}")) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
            return ECSVM_ERROR_MEMORY;
        }
        return ECSVM_OK;
    }

    if (!ecsvm_ecsbin_text_buffer_append(buffer, "{\n")) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
        return ECSVM_ERROR_MEMORY;
    }

    child_index = node->first_child;
    while (child_index != 0u) {
        if (!ecsvm_ecsbin_text_buffer_append_indent(buffer, indent + 1u)) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
            return ECSVM_ERROR_MEMORY;
        }

        {
            const ecsvm_ecsbin_ast_node_t *previous;

            previous = NULL;
        while (child_index != 0u) {
            const ecsvm_ecsbin_ast_node_t *child;
            uint32_t next_index;
            ecsvm_status_t status;

            child = ecsvm_ecsbin_ast_node(ast, child_index, error_message, error_message_capacity);
            if (child == NULL) {
                return ECSVM_ERROR_ARGUMENT;
            }
            if (!ecsvm_ecsbin_ast_child_link_is_valid(ast, child->next_sibling)) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function body ast sibling link is invalid");
                return ECSVM_ERROR_ARGUMENT;
            }

            next_index = child->next_sibling;
            if (child->kind == ECSVM_ECSBIN_AST_NODE_BLOCK) {
                if (previous != NULL &&
                    !ecsvm_ecsbin_text_buffer_append_char(buffer, ' ')) {
                    ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
                    return ECSVM_ERROR_MEMORY;
                }
                status = ecsvm_ecsbin_render_ast_block(
                    ast,
                    child_index,
                    indent + 1u,
                    buffer,
                    error_message,
                    error_message_capacity
                );
                if (status != ECSVM_OK) {
                    return status;
                }
                child_index = next_index;
                break;
            }

            if (previous != NULL &&
                !ecsvm_ecsbin_node_needs_no_leading_space(child) &&
                !ecsvm_ecsbin_node_needs_no_trailing_space(previous) &&
                !ecsvm_ecsbin_text_buffer_append_char(buffer, ' ')) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
                return ECSVM_ERROR_MEMORY;
            }

            status = ecsvm_ecsbin_render_ast_inline_node(
                ast,
                child_index,
                buffer,
                error_message,
                error_message_capacity
            );
            if (status != ECSVM_OK) {
                return status;
            }

            child_index = next_index;
            if (child->kind == ECSVM_ECSBIN_AST_NODE_TOKEN &&
                child->token_kind == ECSVM_ECSBIN_TOKEN_SEMICOLON) {
                break;
            }
            previous = child;
        }
        }

        if (!ecsvm_ecsbin_text_buffer_append_char(buffer, '\n')) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
            return ECSVM_ERROR_MEMORY;
        }
    }

    if (!ecsvm_ecsbin_text_buffer_append_indent(buffer, indent) ||
        !ecsvm_ecsbin_text_buffer_append_char(buffer, '}')) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
        return ECSVM_ERROR_MEMORY;
    }

    return ECSVM_OK;
}

static ecsvm_status_t ecsvm_ecsbin_render_ast_inline_node(
    const ecsvm_ecsbin_ast_blob_t *ast,
    uint32_t node_index,
    ecsvm_ecsbin_text_buffer_t *buffer,
    char *error_message,
    size_t error_message_capacity
)
{
    const ecsvm_ecsbin_ast_node_t *node;

    node = ecsvm_ecsbin_ast_node(ast, node_index, error_message, error_message_capacity);
    if (node == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    if (!ecsvm_ecsbin_ast_child_link_is_valid(ast, node->first_child) ||
        !ecsvm_ecsbin_ast_child_link_is_valid(ast, node->last_child) ||
        !ecsvm_ecsbin_ast_child_link_is_valid(ast, node->next_sibling)) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function body ast links are invalid");
        return ECSVM_ERROR_ARGUMENT;
    }

    if (node->kind == ECSVM_ECSBIN_AST_NODE_TOKEN) {
        if ((size_t)node->text_offset + (size_t)node->text_length > ast->text_length) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function body token text is out of bounds");
            return ECSVM_ERROR_ARGUMENT;
        }

        if (!ecsvm_ecsbin_text_buffer_append_range(
                buffer,
                (const char *)(ast->text_data + node->text_offset),
                (size_t)node->text_length
            )) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
            return ECSVM_ERROR_MEMORY;
        }
        return ECSVM_OK;
    }

    if (node->kind == ECSVM_ECSBIN_AST_NODE_GROUP_PAREN) {
        if (!ecsvm_ecsbin_text_buffer_append_char(buffer, '(')) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
            return ECSVM_ERROR_MEMORY;
        }
        if (node->first_child != 0u) {
            ecsvm_status_t status;

            status = ecsvm_ecsbin_render_ast_inline(
                ast,
                node->first_child,
                buffer,
                error_message,
                error_message_capacity
            );
            if (status != ECSVM_OK) {
                return status;
            }
        }
        if (!ecsvm_ecsbin_text_buffer_append_char(buffer, ')')) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
            return ECSVM_ERROR_MEMORY;
        }
        return ECSVM_OK;
    }

    if (node->kind == ECSVM_ECSBIN_AST_NODE_GROUP_BRACKET) {
        if (!ecsvm_ecsbin_text_buffer_append_char(buffer, '[')) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
            return ECSVM_ERROR_MEMORY;
        }
        if (node->first_child != 0u) {
            ecsvm_status_t status;

            status = ecsvm_ecsbin_render_ast_inline(
                ast,
                node->first_child,
                buffer,
                error_message,
                error_message_capacity
            );
            if (status != ECSVM_OK) {
                return status;
            }
        }
        if (!ecsvm_ecsbin_text_buffer_append_char(buffer, ']')) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
            return ECSVM_ERROR_MEMORY;
        }
        return ECSVM_OK;
    }

    if (node->kind == ECSVM_ECSBIN_AST_NODE_BLOCK) {
        return ecsvm_ecsbin_render_ast_block(
            ast,
            node_index,
            0u,
            buffer,
            error_message,
            error_message_capacity
        );
    }

    ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function body ast node kind is not supported");
    return ECSVM_ERROR_ARGUMENT;
}

static ecsvm_status_t ecsvm_ecsbin_render_ast_inline(
    const ecsvm_ecsbin_ast_blob_t *ast,
    uint32_t child_index,
    ecsvm_ecsbin_text_buffer_t *buffer,
    char *error_message,
    size_t error_message_capacity
)
{
    int first;
    const ecsvm_ecsbin_ast_node_t *previous;

    first = 1;
    previous = NULL;
    while (child_index != 0u) {
        const ecsvm_ecsbin_ast_node_t *node;
        ecsvm_status_t status;

        node = ecsvm_ecsbin_ast_node(ast, child_index, error_message, error_message_capacity);
        if (node == NULL) {
            return ECSVM_ERROR_ARGUMENT;
        }

        if (!first &&
            !ecsvm_ecsbin_node_needs_no_leading_space(node) &&
            !ecsvm_ecsbin_node_needs_no_trailing_space(previous) &&
            !ecsvm_ecsbin_text_buffer_append_char(buffer, ' ')) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
            return ECSVM_ERROR_MEMORY;
        }

        status = ecsvm_ecsbin_render_ast_inline_node(
            ast,
            child_index,
            buffer,
            error_message,
            error_message_capacity
        );
        if (status != ECSVM_OK) {
            return status;
        }

        previous = node;
        child_index = node->next_sibling;
        first = 0;
    }

    return ECSVM_OK;
}

ecsvm_status_t ecsvm_ecsbin_decompile_function_body(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_function_ref_t *function_ref,
    char **out_source,
    char *error_message,
    size_t error_message_capacity
)
{
    const ecsvm_ecsbin_blob_t *blob;
    ecsvm_ecsbin_ast_blob_t ast;
    ecsvm_ecsbin_text_buffer_t buffer;
    ecsvm_status_t status;

    ecsvm_ecsbin_set_error(error_message, error_message_capacity, NULL);
    if (module == NULL || function_ref == NULL || out_source == NULL) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "module, function reference, and output source are required");
        return ECSVM_ERROR_ARGUMENT;
    }

    *out_source = NULL;
    if (function_ref->body_blob_id == 0u) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function does not contain a body");
        return ECSVM_ERROR_NOT_FOUND;
    }

    blob = ecsvm_ecsbin_blob_ref(module, function_ref->body_blob_id);
    status = ecsvm_ecsbin_parse_ast_blob(blob, &ast, error_message, error_message_capacity);
    if (status != ECSVM_OK) {
        return status;
    }

    memset(&buffer, 0, sizeof(buffer));
    status = ecsvm_ecsbin_render_ast_block(
        &ast,
        ast.nodes[0].first_child,
        0u,
        &buffer,
        error_message,
        error_message_capacity
    );
    if (status != ECSVM_OK) {
        free(buffer.data);
        return status;
    }

    *out_source = buffer.data;
    return ECSVM_OK;
}

const ecsvm_ecsbin_struct_def_t *ecsvm_ecsbin_find_struct(
    const ecsvm_ecsbin_module_t *module,
    const char *qualified_name
)
{
    size_t index;

    if (module == NULL || qualified_name == NULL) {
        return NULL;
    }

    for (index = 0u; index < module->struct_def_count; ++index) {
        const ecsvm_ecsbin_type_ref_t *type_ref;

        type_ref = ecsvm_ecsbin_type_ref(module, module->struct_defs[index].type_id);
        if (type_ref != NULL &&
            type_ref->qualified_name != NULL &&
            strcmp(type_ref->qualified_name, qualified_name) == 0) {
            return &module->struct_defs[index];
        }
    }

    return NULL;
}

int ecsvm_ecsbin_struct_is_component(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_struct_def_t *definition
)
{
    size_t attribute_index;

    if (module == NULL || definition == NULL) {
        return 0;
    }

    if ((definition->flags & ECSVM_ECSBIN_STRUCT_FLAG_COMPONENT) != 0u) {
        return 1;
    }

    for (attribute_index = 0u; attribute_index < definition->attribute_count; ++attribute_index) {
        const ecsvm_ecsbin_attribute_t *attribute;
        const ecsvm_ecsbin_type_ref_t *type_ref;

        if (definition->attribute_start == 0u ||
            definition->attribute_start - 1u + attribute_index >= module->attribute_count) {
            return 0;
        }

        attribute = &module->attributes[definition->attribute_start - 1u + attribute_index];
        type_ref = ecsvm_ecsbin_type_ref(module, attribute->type_id);
        if (type_ref != NULL &&
            type_ref->qualified_name != NULL &&
            strcmp(type_ref->qualified_name, "core.Component") == 0) {
            return 1;
        }
    }

    return 0;
}

ecsvm_status_t ecsvm_ecsbin_register_components(
    ecsvm_engine_t *engine,
    const ecsvm_ecsbin_module_t *module
)
{
    size_t index;

    if (engine == NULL || module == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    for (index = 0u; index < module->struct_def_count; ++index) {
        const ecsvm_ecsbin_struct_def_t *definition;
        const ecsvm_ecsbin_type_ref_t *type_ref;
        ecsvm_component_desc_t desc;
        ecsvm_status_t status;

        definition = &module->struct_defs[index];
        if (!ecsvm_ecsbin_struct_is_component(module, definition)) {
            continue;
        }

        type_ref = ecsvm_ecsbin_type_ref(module, definition->type_id);
        if (type_ref == NULL || type_ref->qualified_name == NULL || definition->size == 0u) {
            return ECSVM_ERROR_ARGUMENT;
        }

        desc.name = type_ref->qualified_name;
        desc.size = definition->size;
        desc.preferred_storage = ECSVM_STORAGE_CONTIGUOUS;
        status = ecsvm_engine_register_component(engine, &desc, NULL);
        if (status != ECSVM_OK) {
            return status;
        }
    }

    return ECSVM_OK;
}

void ecsvm_ecsbin_unload(ecsvm_ecsbin_module_t *module)
{
    size_t index;

    if (module == NULL) {
        return;
    }

    for (index = 0u; index < module->type_ref_count; ++index) {
        free(module->type_refs[index].namespace_name);
        free(module->type_refs[index].name);
        free(module->type_refs[index].qualified_name);
    }

    for (index = 0u; index < module->field_ref_count; ++index) {
        free(module->field_refs[index].name);
    }

    for (index = 0u; index < module->function_ref_count; ++index) {
        free(module->function_refs[index].namespace_name);
        free(module->function_refs[index].name);
        free(module->function_refs[index].qualified_name);
    }

    for (index = 0u; index < module->parameter_count; ++index) {
        free(module->parameters[index].name);
    }

    for (index = 0u; index < module->attribute_count; ++index) {
        free(module->attributes[index].data);
    }

    for (index = 0u; index < module->blob_count; ++index) {
        free(module->blobs[index].data);
    }

    free(module->type_refs);
    free(module->field_refs);
    free(module->function_refs);
    free(module->parameters);
    free(module->struct_defs);
    free(module->field_defs);
    free(module->attributes);
    free(module->blobs);
    memset(module, 0, sizeof(*module));
}

ecsvm_status_t ecsvm_ecsbin_load(
    const char *path,
    ecsvm_ecsbin_module_t *out_module,
    char *error_message,
    size_t error_message_capacity
)
{
    FILE *file;
    ecsvm_ecsbin_module_t module;
    ecsvm_ecsbin_header_t header;
    ecsvm_ecsbin_header_prefix_t prefix;
    uint64_t file_size;
    uint64_t blob_data_offset;
    size_t index;
    unsigned char *visit_state;
    ecsvm_status_t status;

    ecsvm_ecsbin_set_error(error_message, error_message_capacity, NULL);
    if (path == NULL || out_module == NULL) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "path and output module are required");
        return ECSVM_ERROR_ARGUMENT;
    }

    memset(&module, 0, sizeof(module));
    file = fopen(path, "rb");
    if (file == NULL) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to open ecsbin file");
        return ECSVM_ERROR_NOT_FOUND;
    }

    memset(&header, 0, sizeof(header));
    if (!ecsvm_ecsbin_read_exact(file, &prefix, sizeof(prefix))) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to read ecsbin header");
        fclose(file);
        return ECSVM_ERROR_ARGUMENT;
    }

    if (memcmp(prefix.magic, "ECSVM", 5u) != 0 ||
        prefix.version[0] != ECSVM_ECSBIN_VERSION_0 ||
        prefix.version[1] != ECSVM_ECSBIN_VERSION_1 ||
        (prefix.version[2] != ECSVM_ECSBIN_VERSION_2_V1 &&
         prefix.version[2] != ECSVM_ECSBIN_VERSION_2)) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "ecsbin header is not recognized");
        fclose(file);
        return ECSVM_ERROR_ARGUMENT;
    }

    if (!ecsvm_ecsbin_seek(file, 0u)) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to seek ecsbin header");
        fclose(file);
        return ECSVM_ERROR_ARGUMENT;
    }

    if (prefix.version[2] == ECSVM_ECSBIN_VERSION_2_V1) {
        ecsvm_ecsbin_header_v1_t header_v1;

        if (!ecsvm_ecsbin_read_exact(file, &header_v1, sizeof(header_v1))) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to read ecsbin header");
            fclose(file);
            return ECSVM_ERROR_ARGUMENT;
        }

        memcpy(header.magic, header_v1.magic, sizeof(header.magic));
        memcpy(header.version, header_v1.version, sizeof(header.version));
        header.type_reference_offset = header_v1.type_reference_offset;
        header.field_reference_offset = header_v1.field_reference_offset;
        header.struct_definition_offset = header_v1.struct_definition_offset;
        header.field_definition_offset = header_v1.field_definition_offset;
        header.attribute_offset = header_v1.attribute_offset;
        header.blob_offset = header_v1.blob_offset;
        header.type_reference_count = header_v1.type_reference_count;
        header.field_reference_count = header_v1.field_reference_count;
        header.struct_definition_count = header_v1.struct_definition_count;
        header.field_definition_count = header_v1.field_definition_count;
        header.attribute_count = header_v1.attribute_count;
        header.blob_count = header_v1.blob_count;
    } else if (!ecsvm_ecsbin_read_exact(file, &header, sizeof(header))) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to read ecsbin header");
        fclose(file);
        return ECSVM_ERROR_ARGUMENT;
    }

    file_size = ecsvm_ecsbin_file_size(file);
    if (file_size < (prefix.version[2] == ECSVM_ECSBIN_VERSION_2_V1
                         ? sizeof(ecsvm_ecsbin_header_v1_t)
                         : sizeof(header))) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "ecsbin file is truncated");
        fclose(file);
        return ECSVM_ERROR_ARGUMENT;
    }

    module.type_ref_count = header.type_reference_count;
    module.field_ref_count = header.field_reference_count;
    module.function_ref_count = header.function_reference_count;
    module.parameter_count = header.parameter_count;
    module.struct_def_count = header.struct_definition_count;
    module.field_def_count = header.field_definition_count;
    module.attribute_count = header.attribute_count;
    module.blob_count = header.blob_count;

    if (module.type_ref_count > 0u) {
        module.type_refs = (ecsvm_ecsbin_type_ref_t *)calloc(
            module.type_ref_count,
            sizeof(*module.type_refs)
        );
    }
    if (module.field_ref_count > 0u) {
        module.field_refs = (ecsvm_ecsbin_field_ref_t *)calloc(
            module.field_ref_count,
            sizeof(*module.field_refs)
        );
    }
    if (module.function_ref_count > 0u) {
        module.function_refs = (ecsvm_ecsbin_function_ref_t *)calloc(
            module.function_ref_count,
            sizeof(*module.function_refs)
        );
    }
    if (module.parameter_count > 0u) {
        module.parameters = (ecsvm_ecsbin_parameter_t *)calloc(
            module.parameter_count,
            sizeof(*module.parameters)
        );
    }
    if (module.struct_def_count > 0u) {
        module.struct_defs = (ecsvm_ecsbin_struct_def_t *)calloc(
            module.struct_def_count,
            sizeof(*module.struct_defs)
        );
    }
    if (module.field_def_count > 0u) {
        module.field_defs = (ecsvm_ecsbin_field_def_t *)calloc(
            module.field_def_count,
            sizeof(*module.field_defs)
        );
    }
    if (module.attribute_count > 0u) {
        module.attributes = (ecsvm_ecsbin_attribute_t *)calloc(
            module.attribute_count,
            sizeof(*module.attributes)
        );
    }
    if (module.blob_count > 0u) {
        module.blobs = (ecsvm_ecsbin_blob_t *)calloc(module.blob_count, sizeof(*module.blobs));
    }

    if ((module.type_ref_count > 0u && module.type_refs == NULL) ||
        (module.field_ref_count > 0u && module.field_refs == NULL) ||
        (module.function_ref_count > 0u && module.function_refs == NULL) ||
        (module.parameter_count > 0u && module.parameters == NULL) ||
        (module.struct_def_count > 0u && module.struct_defs == NULL) ||
        (module.field_def_count > 0u && module.field_defs == NULL) ||
        (module.attribute_count > 0u && module.attributes == NULL) ||
        (module.blob_count > 0u && module.blobs == NULL)) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while allocating ecsbin tables");
        fclose(file);
        ecsvm_ecsbin_unload(&module);
        return ECSVM_ERROR_MEMORY;
    }

    blob_data_offset = header.blob_offset + (module.blob_count * sizeof(ecsvm_ecsbin_blob_disk_t));
    if (blob_data_offset > file_size) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "blob data offset is out of bounds");
        fclose(file);
        ecsvm_ecsbin_unload(&module);
        return ECSVM_ERROR_ARGUMENT;
    }

    if (module.blob_count > 0u) {
        ecsvm_ecsbin_blob_disk_t *disk_blobs;

        disk_blobs = (ecsvm_ecsbin_blob_disk_t *)malloc(
            module.blob_count * sizeof(*disk_blobs)
        );
        if (disk_blobs == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while loading blob references");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_MEMORY;
        }

        if (!ecsvm_ecsbin_seek(file, header.blob_offset) ||
            !ecsvm_ecsbin_read_exact(
                file,
                disk_blobs,
                module.blob_count * sizeof(*disk_blobs)
            )) {
            free(disk_blobs);
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to read blob references");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_ARGUMENT;
        }

        for (index = 0u; index < module.blob_count; ++index) {
            module.blobs[index].offset = disk_blobs[index].offset;
            module.blobs[index].length = disk_blobs[index].length;
            if (module.blobs[index].offset > file_size ||
                module.blobs[index].length > file_size ||
                blob_data_offset + module.blobs[index].offset > file_size ||
                module.blobs[index].length >
                    file_size - (blob_data_offset + module.blobs[index].offset)) {
                free(disk_blobs);
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "blob range is out of bounds");
                fclose(file);
                ecsvm_ecsbin_unload(&module);
                return ECSVM_ERROR_ARGUMENT;
            }

            if (module.blobs[index].length > 0u) {
                module.blobs[index].data = (unsigned char *)malloc(
                    (size_t)module.blobs[index].length
                );
                if (module.blobs[index].data == NULL) {
                    free(disk_blobs);
                    ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while loading blob data");
                    fclose(file);
                    ecsvm_ecsbin_unload(&module);
                    return ECSVM_ERROR_MEMORY;
                }

                if (!ecsvm_ecsbin_seek(file, blob_data_offset + module.blobs[index].offset) ||
                    !ecsvm_ecsbin_read_exact(
                        file,
                        module.blobs[index].data,
                        (size_t)module.blobs[index].length
                    )) {
                    free(disk_blobs);
                    ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to read blob payload");
                    fclose(file);
                    ecsvm_ecsbin_unload(&module);
                    return ECSVM_ERROR_ARGUMENT;
                }
            }
        }

        free(disk_blobs);
    }

    if (module.type_ref_count > 0u) {
        ecsvm_ecsbin_type_ref_disk_t *disk_refs;

        disk_refs = (ecsvm_ecsbin_type_ref_disk_t *)malloc(
            module.type_ref_count * sizeof(*disk_refs)
        );
        if (disk_refs == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while loading type references");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_MEMORY;
        }

        if (!ecsvm_ecsbin_seek(file, header.type_reference_offset) ||
            !ecsvm_ecsbin_read_exact(
                file,
                disk_refs,
                module.type_ref_count * sizeof(*disk_refs)
            )) {
            free(disk_refs);
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to read type references");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_ARGUMENT;
        }

        for (index = 0u; index < module.type_ref_count; ++index) {
            char *namespace_name;
            char *name;
            size_t namespace_length;
            size_t name_length;

            namespace_name = ecsvm_ecsbin_blob_string(&module, disk_refs[index].namespace_blob_id);
            name = ecsvm_ecsbin_blob_string(&module, disk_refs[index].name_blob_id);
            if (namespace_name == NULL || name == NULL) {
                free(namespace_name);
                free(name);
                free(disk_refs);
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "type reference uses an invalid blob");
                fclose(file);
                ecsvm_ecsbin_unload(&module);
                return ECSVM_ERROR_ARGUMENT;
            }

            module.type_refs[index].namespace_name = namespace_name;
            module.type_refs[index].name = name;
            namespace_length = strlen(namespace_name);
            name_length = strlen(name);
            module.type_refs[index].qualified_name = (char *)malloc(
                namespace_length + name_length + 2u
            );
            if (module.type_refs[index].qualified_name == NULL) {
                free(disk_refs);
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while composing type names");
                fclose(file);
                ecsvm_ecsbin_unload(&module);
                return ECSVM_ERROR_MEMORY;
            }

            if (namespace_length == 0u) {
                (void)snprintf(
                    module.type_refs[index].qualified_name,
                    name_length + 1u,
                    "%s",
                    name
                );
            } else {
                (void)snprintf(
                    module.type_refs[index].qualified_name,
                    namespace_length + name_length + 2u,
                    "%s.%s",
                    namespace_name,
                    name
                );
            }
        }

        free(disk_refs);
    }

    if (module.field_ref_count > 0u) {
        ecsvm_ecsbin_field_ref_disk_t *disk_refs;

        disk_refs = (ecsvm_ecsbin_field_ref_disk_t *)malloc(
            module.field_ref_count * sizeof(*disk_refs)
        );
        if (disk_refs == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while loading field references");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_MEMORY;
        }

        if (!ecsvm_ecsbin_seek(file, header.field_reference_offset) ||
            !ecsvm_ecsbin_read_exact(
                file,
                disk_refs,
                module.field_ref_count * sizeof(*disk_refs)
            )) {
            free(disk_refs);
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to read field references");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_ARGUMENT;
        }

        for (index = 0u; index < module.field_ref_count; ++index) {
            module.field_refs[index].name = ecsvm_ecsbin_blob_string(
                &module,
                disk_refs[index].name_blob_id
            );
            module.field_refs[index].type_id = disk_refs[index].type_id;
            if (module.field_refs[index].name == NULL ||
                module.field_refs[index].type_id == 0u ||
                module.field_refs[index].type_id > module.type_ref_count) {
                free(disk_refs);
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "field reference is invalid");
                fclose(file);
                ecsvm_ecsbin_unload(&module);
                return ECSVM_ERROR_ARGUMENT;
            }
        }

        free(disk_refs);
    }

    if (module.function_ref_count > 0u) {
        ecsvm_ecsbin_function_ref_disk_t *disk_refs;

        disk_refs = (ecsvm_ecsbin_function_ref_disk_t *)malloc(
            module.function_ref_count * sizeof(*disk_refs)
        );
        if (disk_refs == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while loading function references");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_MEMORY;
        }

        if (!ecsvm_ecsbin_seek(file, header.function_reference_offset) ||
            !ecsvm_ecsbin_read_exact(
                file,
                disk_refs,
                module.function_ref_count * sizeof(*disk_refs)
            )) {
            free(disk_refs);
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to read function references");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_ARGUMENT;
        }

        for (index = 0u; index < module.function_ref_count; ++index) {
            char *namespace_name;
            char *name;
            char *qualified_name;

            namespace_name = ecsvm_ecsbin_blob_string(&module, disk_refs[index].namespace_blob_id);
            name = ecsvm_ecsbin_blob_string(&module, disk_refs[index].name_blob_id);
            qualified_name = ecsvm_ecsbin_compose_qualified_name(namespace_name, name);
            if (namespace_name == NULL || name == NULL || qualified_name == NULL) {
                free(namespace_name);
                free(name);
                free(qualified_name);
                free(disk_refs);
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function reference uses an invalid blob");
                fclose(file);
                ecsvm_ecsbin_unload(&module);
                return ECSVM_ERROR_ARGUMENT;
            }

            module.function_refs[index].namespace_name = namespace_name;
            module.function_refs[index].name = name;
            module.function_refs[index].qualified_name = qualified_name;
            module.function_refs[index].parameter_start = disk_refs[index].parameter_start;
            module.function_refs[index].parameter_count = disk_refs[index].parameter_count;
            module.function_refs[index].attribute_start = disk_refs[index].attribute_start;
            module.function_refs[index].attribute_count = disk_refs[index].attribute_count;
            module.function_refs[index].body_blob_id = disk_refs[index].body_blob_id;
            if ((module.function_refs[index].body_blob_id != 0u &&
                 module.function_refs[index].body_blob_id > module.blob_count)) {
                free(disk_refs);
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function reference is invalid");
                fclose(file);
                ecsvm_ecsbin_unload(&module);
                return ECSVM_ERROR_ARGUMENT;
            }
        }

        free(disk_refs);
    }

    if (module.parameter_count > 0u) {
        ecsvm_ecsbin_parameter_disk_t *disk_parameters;

        disk_parameters = (ecsvm_ecsbin_parameter_disk_t *)malloc(
            module.parameter_count * sizeof(*disk_parameters)
        );
        if (disk_parameters == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while loading parameters");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_MEMORY;
        }

        if (!ecsvm_ecsbin_seek(file, header.parameter_offset) ||
            !ecsvm_ecsbin_read_exact(
                file,
                disk_parameters,
                module.parameter_count * sizeof(*disk_parameters)
            )) {
            free(disk_parameters);
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to read parameters");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_ARGUMENT;
        }

        for (index = 0u; index < module.parameter_count; ++index) {
            module.parameters[index].name = ecsvm_ecsbin_blob_string(
                &module,
                disk_parameters[index].name_blob_id
            );
            module.parameters[index].type_id = disk_parameters[index].type_id;
            module.parameters[index].attribute_start = disk_parameters[index].attribute_start;
            module.parameters[index].attribute_count = disk_parameters[index].attribute_count;
            module.parameters[index].default_value_blob_id = disk_parameters[index].default_value_blob_id;
            if (module.parameters[index].name == NULL ||
                module.parameters[index].type_id == 0u ||
                module.parameters[index].type_id > module.type_ref_count ||
                (module.parameters[index].default_value_blob_id != 0u &&
                 module.parameters[index].default_value_blob_id > module.blob_count)) {
                free(disk_parameters);
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "parameter is invalid");
                fclose(file);
                ecsvm_ecsbin_unload(&module);
                return ECSVM_ERROR_ARGUMENT;
            }
        }

        free(disk_parameters);
    }

    if (module.struct_def_count > 0u) {
        ecsvm_ecsbin_struct_def_disk_t *disk_defs;

        disk_defs = (ecsvm_ecsbin_struct_def_disk_t *)malloc(
            module.struct_def_count * sizeof(*disk_defs)
        );
        if (disk_defs == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while loading struct definitions");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_MEMORY;
        }

        if (!ecsvm_ecsbin_seek(file, header.struct_definition_offset) ||
            !ecsvm_ecsbin_read_exact(
                file,
                disk_defs,
                module.struct_def_count * sizeof(*disk_defs)
            )) {
            free(disk_defs);
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to read struct definitions");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_ARGUMENT;
        }

        for (index = 0u; index < module.struct_def_count; ++index) {
            module.struct_defs[index].type_id = disk_defs[index].type_id;
            module.struct_defs[index].flags = disk_defs[index].flags;
            module.struct_defs[index].field_start = disk_defs[index].field_start;
            module.struct_defs[index].field_count = disk_defs[index].field_count;
            module.struct_defs[index].attribute_start = disk_defs[index].attribute_start;
            module.struct_defs[index].attribute_count = disk_defs[index].attribute_count;
            if (module.struct_defs[index].type_id == 0u ||
                module.struct_defs[index].type_id > module.type_ref_count) {
                free(disk_defs);
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "struct definition has an invalid type id");
                fclose(file);
                ecsvm_ecsbin_unload(&module);
                return ECSVM_ERROR_ARGUMENT;
            }
        }

        free(disk_defs);
    }

    if (module.field_def_count > 0u) {
        ecsvm_ecsbin_field_def_disk_t *disk_defs;

        disk_defs = (ecsvm_ecsbin_field_def_disk_t *)malloc(
            module.field_def_count * sizeof(*disk_defs)
        );
        if (disk_defs == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while loading field definitions");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_MEMORY;
        }

        if (!ecsvm_ecsbin_seek(file, header.field_definition_offset) ||
            !ecsvm_ecsbin_read_exact(
                file,
                disk_defs,
                module.field_def_count * sizeof(*disk_defs)
            )) {
            free(disk_defs);
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to read field definitions");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_ARGUMENT;
        }

        for (index = 0u; index < module.field_def_count; ++index) {
            module.field_defs[index].field_id = disk_defs[index].field_id;
            module.field_defs[index].attribute_start = disk_defs[index].attribute_start;
            module.field_defs[index].attribute_count = disk_defs[index].attribute_count;
            if (module.field_defs[index].field_id == 0u ||
                module.field_defs[index].field_id > module.field_ref_count) {
                free(disk_defs);
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "field definition has an invalid field id");
                fclose(file);
                ecsvm_ecsbin_unload(&module);
                return ECSVM_ERROR_ARGUMENT;
            }
        }

        free(disk_defs);
    }

    if (module.attribute_count > 0u) {
        ecsvm_ecsbin_attribute_disk_t *disk_attributes;

        disk_attributes = (ecsvm_ecsbin_attribute_disk_t *)malloc(
            module.attribute_count * sizeof(*disk_attributes)
        );
        if (disk_attributes == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while loading attributes");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_MEMORY;
        }

        if (!ecsvm_ecsbin_seek(file, header.attribute_offset) ||
            !ecsvm_ecsbin_read_exact(
                file,
                disk_attributes,
                module.attribute_count * sizeof(*disk_attributes)
            )) {
            free(disk_attributes);
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to read attributes");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_ARGUMENT;
        }

        for (index = 0u; index < module.attribute_count; ++index) {
            module.attributes[index].type_id = disk_attributes[index].type_id;
            module.attributes[index].data = ecsvm_ecsbin_blob_string(
                &module,
                disk_attributes[index].data_blob_id
            );
            if (module.attributes[index].type_id == 0u ||
                module.attributes[index].type_id > module.type_ref_count ||
                module.attributes[index].data == NULL) {
                free(disk_attributes);
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "attribute is invalid");
                fclose(file);
                ecsvm_ecsbin_unload(&module);
                return ECSVM_ERROR_ARGUMENT;
            }
        }

        free(disk_attributes);
    }

    for (index = 0u; index < module.field_def_count; ++index) {
        if (!ecsvm_ecsbin_range_is_valid(
                module.attribute_count,
                module.field_defs[index].attribute_start,
                module.field_defs[index].attribute_count
            )) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "field definition attribute range is invalid");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_ARGUMENT;
        }
    }

    for (index = 0u; index < module.parameter_count; ++index) {
        if (!ecsvm_ecsbin_range_is_valid(
                module.attribute_count,
                module.parameters[index].attribute_start,
                module.parameters[index].attribute_count
            )) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "parameter attribute range is invalid");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_ARGUMENT;
        }
    }

    for (index = 0u; index < module.struct_def_count; ++index) {
        if (!ecsvm_ecsbin_range_is_valid(
                module.field_ref_count,
                module.struct_defs[index].field_start,
                module.struct_defs[index].field_count
            ) ||
            !ecsvm_ecsbin_range_is_valid(
                module.attribute_count,
                module.struct_defs[index].attribute_start,
                module.struct_defs[index].attribute_count
            )) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "struct definition range is invalid");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_ARGUMENT;
        }
    }

    for (index = 0u; index < module.function_ref_count; ++index) {
        if (!ecsvm_ecsbin_range_is_valid(
                module.parameter_count,
                module.function_refs[index].parameter_start,
                module.function_refs[index].parameter_count
            ) ||
            !ecsvm_ecsbin_range_is_valid(
                module.attribute_count,
                module.function_refs[index].attribute_start,
                module.function_refs[index].attribute_count
            )) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function reference range is invalid");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_ARGUMENT;
        }
    }

    fclose(file);

    visit_state = NULL;
    if (module.struct_def_count > 0u) {
        visit_state = (unsigned char *)calloc(module.struct_def_count, sizeof(*visit_state));
        if (visit_state == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while computing struct layouts");
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_MEMORY;
        }
    }

    status = ECSVM_OK;
    for (index = 0u; index < module.struct_def_count; ++index) {
        status = ecsvm_ecsbin_compute_struct_layout(
            &module,
            index,
            visit_state,
            error_message,
            error_message_capacity
        );
        if (status != ECSVM_OK) {
            free(visit_state);
            ecsvm_ecsbin_unload(&module);
            return status;
        }
    }

    free(visit_state);
    *out_module = module;
    return ECSVM_OK;
}
