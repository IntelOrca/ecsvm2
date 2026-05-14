#include "bin_internal.h"

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

typedef struct ecsvm_string_list {
    const char **items;
    size_t count;
    size_t capacity;
} ecsvm_string_list_t;

static void ecsvm_string_list_free(ecsvm_string_list_t *list)
{
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static int ecsvm_string_list_push_unique(ecsvm_string_list_t *list, const char *value)
{
    size_t index;
    const char **items;
    size_t capacity;
    const char *normalized;

    normalized = value != NULL ? value : "";
    for (index = 0u; index < list->count; ++index) {
        if (strcmp(list->items[index], normalized) == 0) {
            return 1;
        }
    }

    if (list->count == list->capacity) {
        capacity = list->capacity == 0u ? 8u : list->capacity * 2u;
        items = (const char **)realloc(list->items, capacity * sizeof(*items));
        if (items == NULL) {
            return 0;
        }
        list->items = items;
        list->capacity = capacity;
    }

    list->items[list->count++] = normalized;
    return 1;
}

static const char *ecsvm_source_builtin_type_name(const char *qualified_name)
{
    if (qualified_name == NULL) {
        return NULL;
    }
    if (strcmp(qualified_name, "core.Entity") == 0) {
        return "entity";
    }
    if (strcmp(qualified_name, "core.Int32") == 0) {
        return "i32";
    }
    if (strcmp(qualified_name, "core.UInt32") == 0) {
        return "u32";
    }
    if (strcmp(qualified_name, "core.Float32") == 0) {
        return "f32";
    }
    if (strcmp(qualified_name, "core.Void") == 0) {
        return "void";
    }
    if (strcmp(qualified_name, "core.Blob") == 0) {
        return "blob";
    }
    if (strcmp(qualified_name, "core.String") == 0) {
        return "string";
    }
    if (strcmp(qualified_name, "core.Bool") == 0) {
        return "bool";
    }
    return NULL;
}

static const char *ecsvm_display_type_name(
    const ecsvm_ecsbin_type_ref_t *type_ref,
    const char *current_namespace
)
{
    const char *builtin_name;

    if (type_ref == NULL) {
        return "<invalid>";
    }

    builtin_name = ecsvm_source_builtin_type_name(type_ref->qualified_name);
    if (builtin_name != NULL) {
        return builtin_name;
    }

    if (current_namespace != NULL &&
        type_ref->namespace_name != NULL &&
        strcmp(type_ref->namespace_name, current_namespace) == 0) {
        return type_ref->name;
    }

    return type_ref->qualified_name;
}

static int ecsvm_append_prefixed_multiline(
    ecsvm_ecsbin_text_buffer_t *buffer,
    const char *text,
    const char *line_prefix
)
{
    const char *cursor;

    cursor = text;
    while (cursor != NULL && *cursor != '\0') {
        if (!ecsvm_ecsbin_text_buffer_append_char(buffer, *cursor)) {
            return 0;
        }
        if (*cursor == '\n' && cursor[1] != '\0' && line_prefix != NULL &&
            !ecsvm_ecsbin_text_buffer_append(buffer, line_prefix)) {
            return 0;
        }
        cursor += 1;
    }
    return 1;
}

static ecsvm_status_t ecsvm_append_decompiled_struct(
    const ecsvm_ecsbin_module_t *module,
    size_t struct_index,
    const char *current_namespace,
    const char *indent,
    ecsvm_ecsbin_text_buffer_t *buffer,
    char *error_message,
    size_t error_message_capacity
)
{
    const ecsvm_ecsbin_struct_def_t *definition;
    const ecsvm_ecsbin_type_ref_t *type_ref;
    size_t attribute_index;
    size_t field_index;
    int is_component;

    definition = &module->struct_defs[struct_index];
    type_ref = ecsvm_ecsbin_type_ref(module, definition->type_id);
    if (type_ref == NULL) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "struct type reference is invalid");
        return ECSVM_ERROR_ARGUMENT;
    }

    is_component = ecsvm_ecsbin_struct_is_component(module, definition);
    for (attribute_index = 0u; attribute_index < definition->attribute_count; ++attribute_index) {
        const ecsvm_ecsbin_attribute_t *attribute;
        const ecsvm_ecsbin_type_ref_t *attribute_type;

        attribute = ecsvm_ecsbin_attribute_ref(module, definition->attribute_start + (uint32_t)attribute_index);
        attribute_type = attribute != NULL ? ecsvm_ecsbin_type_ref(module, attribute->type_id) : NULL;
        if (attribute_type == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "struct attribute is invalid");
            return ECSVM_ERROR_ARGUMENT;
        }
        if (is_component && strcmp(attribute_type->qualified_name, "core.Component") == 0) {
            continue;
        }
        if (!ecsvm_ecsbin_text_buffer_append(buffer, indent) ||
            !ecsvm_ecsbin_text_buffer_append_char(buffer, '[') ||
            !ecsvm_ecsbin_text_buffer_append(buffer, ecsvm_display_type_name(attribute_type, current_namespace)) ||
            !ecsvm_ecsbin_text_buffer_append(buffer, "]\n")) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
            return ECSVM_ERROR_MEMORY;
        }
    }

    if (!ecsvm_ecsbin_text_buffer_append(buffer, indent) ||
        !ecsvm_ecsbin_text_buffer_append(buffer, is_component ? "component " : "struct ") ||
        !ecsvm_ecsbin_text_buffer_append(buffer, type_ref->name) ||
        !ecsvm_ecsbin_text_buffer_append(buffer, " {\n")) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
        return ECSVM_ERROR_MEMORY;
    }

    for (field_index = 0u; field_index < definition->field_count; ++field_index) {
        const ecsvm_ecsbin_field_ref_t *field_ref;
        const ecsvm_ecsbin_type_ref_t *field_type;

        if (definition->field_start == 0u ||
            definition->field_start - 1u + field_index >= module->field_ref_count) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "struct field range is invalid");
            return ECSVM_ERROR_ARGUMENT;
        }
        field_ref = &module->field_refs[definition->field_start - 1u + field_index];
        field_type = ecsvm_ecsbin_type_ref(module, field_ref->type_id);
        if (field_type == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "field type reference is invalid");
            return ECSVM_ERROR_ARGUMENT;
        }
        if (!ecsvm_ecsbin_text_buffer_append(buffer, indent) ||
            !ecsvm_ecsbin_text_buffer_append(buffer, "    ") ||
            !ecsvm_ecsbin_text_buffer_append(buffer, field_ref->name) ||
            !ecsvm_ecsbin_text_buffer_append(buffer, ": ") ||
            !ecsvm_ecsbin_text_buffer_append(buffer, ecsvm_display_type_name(field_type, current_namespace)) ||
            !ecsvm_ecsbin_text_buffer_append(buffer, ";\n")) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
            return ECSVM_ERROR_MEMORY;
        }
    }

    if (!ecsvm_ecsbin_text_buffer_append(buffer, indent) ||
        !ecsvm_ecsbin_text_buffer_append(buffer, "}\n")) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
        return ECSVM_ERROR_MEMORY;
    }
    return ECSVM_OK;
}

static ecsvm_status_t ecsvm_append_decompiled_function(
    const ecsvm_ecsbin_module_t *module,
    size_t function_index,
    const char *current_namespace,
    const char *indent,
    ecsvm_ecsbin_text_buffer_t *buffer,
    char *error_message,
    size_t error_message_capacity
)
{
    const ecsvm_ecsbin_function_ref_t *function_ref;
    const ecsvm_ecsbin_type_ref_t *return_type;
    size_t parameter_index;

    function_ref = &module->function_refs[function_index];
    return_type = ecsvm_ecsbin_function_return_type(module, function_ref);
    if (return_type == NULL) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function return type is invalid");
        return ECSVM_ERROR_ARGUMENT;
    }

    if (!ecsvm_ecsbin_text_buffer_append(buffer, indent) ||
        !ecsvm_ecsbin_text_buffer_append(buffer, "fn ") ||
        !ecsvm_ecsbin_text_buffer_append(buffer, function_ref->name) ||
        !ecsvm_ecsbin_text_buffer_append_char(buffer, '(')) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
        return ECSVM_ERROR_MEMORY;
    }

    for (parameter_index = 0u; parameter_index < function_ref->parameter_count; ++parameter_index) {
        const ecsvm_ecsbin_parameter_t *parameter;
        const ecsvm_ecsbin_type_ref_t *parameter_type;

        parameter = ecsvm_ecsbin_parameter_ref(module, function_ref->parameter_start + (uint32_t)parameter_index);
        parameter_type = parameter != NULL ? ecsvm_ecsbin_type_ref(module, parameter->type_id) : NULL;
        if (parameter == NULL || parameter_type == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function parameter is invalid");
            return ECSVM_ERROR_ARGUMENT;
        }
        if (parameter_index > 0u && !ecsvm_ecsbin_text_buffer_append(buffer, ", ")) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
            return ECSVM_ERROR_MEMORY;
        }
        if (!ecsvm_ecsbin_text_buffer_append(buffer, parameter->name) ||
            !ecsvm_ecsbin_text_buffer_append(buffer, ": ") ||
            !ecsvm_ecsbin_text_buffer_append(buffer, ecsvm_display_type_name(parameter_type, current_namespace))) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
            return ECSVM_ERROR_MEMORY;
        }
        if (parameter->default_value_blob_id != 0u) {
            const ecsvm_ecsbin_blob_t *default_value_blob;
            default_value_blob = ecsvm_ecsbin_blob_ref(module, parameter->default_value_blob_id);
            if (default_value_blob == NULL) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "parameter default value blob is invalid");
                return ECSVM_ERROR_ARGUMENT;
            }
            if (!ecsvm_ecsbin_text_buffer_append(buffer, " = ") ||
                !ecsvm_ecsbin_text_buffer_append_range(buffer, (const char *)default_value_blob->data, (size_t)default_value_blob->length)) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
                return ECSVM_ERROR_MEMORY;
            }
        }
    }

    if (!ecsvm_ecsbin_text_buffer_append_char(buffer, ')')) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
        return ECSVM_ERROR_MEMORY;
    }
    if (strcmp(return_type->qualified_name, "core.Void") != 0 &&
        (!ecsvm_ecsbin_text_buffer_append(buffer, ": ") ||
         !ecsvm_ecsbin_text_buffer_append(buffer, ecsvm_display_type_name(return_type, current_namespace)))) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
        return ECSVM_ERROR_MEMORY;
    }

    if (function_ref->body_blob_id == 0u) {
        if (!ecsvm_ecsbin_text_buffer_append(buffer, ";\n")) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
            return ECSVM_ERROR_MEMORY;
        }
    } else {
        char *body_source;
        ecsvm_status_t status;

        body_source = NULL;
        status = ecsvm_ecsbin_decompile_function_body(module, function_ref, &body_source, error_message, error_message_capacity);
        if (status != ECSVM_OK) {
            free(body_source);
            return status;
        }
        if (!ecsvm_ecsbin_text_buffer_append(buffer, " ") ||
            !ecsvm_append_prefixed_multiline(buffer, body_source, indent) ||
            !ecsvm_ecsbin_text_buffer_append_char(buffer, '\n')) {
            free(body_source);
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
            return ECSVM_ERROR_MEMORY;
        }
        free(body_source);
    }

    return ECSVM_OK;
}

ecsvm_status_t ecsvm_ecsbin_decompile_module(
    const ecsvm_ecsbin_module_t *module,
    char **out_source,
    char *error_message,
    size_t error_message_capacity,
    ecsvm_diagnostic_t *diagnostic
)
{
    ecsvm_string_list_t namespaces;
    ecsvm_ecsbin_text_buffer_t buffer;
    size_t index;

    if (diagnostic != NULL) {
        ecsvm_diagnostic_clear(diagnostic);
    }
    if (module == NULL || out_source == NULL) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "module and output source are required");
        ecsvm_diagnostic_set(diagnostic, NULL, 0u, 0u, ECSVM_DIAGNOSTIC_ARGUMENT, error_message);
        return ECSVM_ERROR_ARGUMENT;
    }

    *out_source = NULL;
    memset(&namespaces, 0, sizeof(namespaces));
    memset(&buffer, 0, sizeof(buffer));

    for (index = 0u; index < module->struct_def_count; ++index) {
        const ecsvm_ecsbin_type_ref_t *type_ref = ecsvm_ecsbin_type_ref(module, module->struct_defs[index].type_id);
        if (type_ref == NULL || !ecsvm_string_list_push_unique(&namespaces, type_ref->namespace_name)) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while grouping namespaces");
            ecsvm_diagnostic_set(diagnostic, NULL, 0u, 0u, ECSVM_DIAGNOSTIC_OUT_OF_MEMORY, error_message);
            ecsvm_string_list_free(&namespaces);
            return ECSVM_ERROR_MEMORY;
        }
    }
    for (index = 0u; index < module->function_ref_count; ++index) {
        if (!ecsvm_string_list_push_unique(&namespaces, module->function_refs[index].namespace_name)) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while grouping namespaces");
            ecsvm_diagnostic_set(diagnostic, NULL, 0u, 0u, ECSVM_DIAGNOSTIC_OUT_OF_MEMORY, error_message);
            ecsvm_string_list_free(&namespaces);
            return ECSVM_ERROR_MEMORY;
        }
    }

    for (index = 0u; index < namespaces.count; ++index) {
        const char *namespace_name = namespaces.items[index];
        const char *indent = namespace_name[0] == '\0' ? "" : "    ";
        size_t struct_index;
        size_t function_index;
        int emitted_anything;

        emitted_anything = 0;
        if (index > 0u && !ecsvm_ecsbin_text_buffer_append_char(&buffer, '\n')) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
            ecsvm_diagnostic_set(diagnostic, NULL, 0u, 0u, ECSVM_DIAGNOSTIC_OUT_OF_MEMORY, error_message);
            goto fail;
        }
        if (namespace_name[0] != '\0' &&
            (!ecsvm_ecsbin_text_buffer_append(&buffer, "namespace ") ||
             !ecsvm_ecsbin_text_buffer_append(&buffer, namespace_name) ||
             !ecsvm_ecsbin_text_buffer_append(&buffer, " {\n"))) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
            ecsvm_diagnostic_set(diagnostic, NULL, 0u, 0u, ECSVM_DIAGNOSTIC_OUT_OF_MEMORY, error_message);
            goto fail;
        }

        for (struct_index = 0u; struct_index < module->struct_def_count; ++struct_index) {
            const ecsvm_ecsbin_type_ref_t *type_ref = ecsvm_ecsbin_type_ref(module, module->struct_defs[struct_index].type_id);
            ecsvm_status_t status;
            if (type_ref == NULL || strcmp(type_ref->namespace_name, namespace_name) != 0) {
                continue;
            }
            if (emitted_anything && !ecsvm_ecsbin_text_buffer_append_char(&buffer, '\n')) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
                ecsvm_diagnostic_set(diagnostic, NULL, 0u, 0u, ECSVM_DIAGNOSTIC_OUT_OF_MEMORY, error_message);
                goto fail;
            }
            status = ecsvm_append_decompiled_struct(module, struct_index, namespace_name, indent, &buffer, error_message, error_message_capacity);
            if (status != ECSVM_OK) {
                ecsvm_diagnostic_set(diagnostic, NULL, 0u, 0u, ECSVM_DIAGNOSTIC_INVALID_BINARY, error_message);
                goto fail;
            }
            emitted_anything = 1;
        }

        for (function_index = 0u; function_index < module->function_ref_count; ++function_index) {
            ecsvm_status_t status;
            if (strcmp(module->function_refs[function_index].namespace_name, namespace_name) != 0) {
                continue;
            }
            if (emitted_anything && !ecsvm_ecsbin_text_buffer_append_char(&buffer, '\n')) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
                ecsvm_diagnostic_set(diagnostic, NULL, 0u, 0u, ECSVM_DIAGNOSTIC_OUT_OF_MEMORY, error_message);
                goto fail;
            }
            status = ecsvm_append_decompiled_function(module, function_index, namespace_name, indent, &buffer, error_message, error_message_capacity);
            if (status != ECSVM_OK) {
                ecsvm_diagnostic_set(diagnostic, NULL, 0u, 0u, ECSVM_DIAGNOSTIC_INVALID_BINARY, error_message);
                goto fail;
            }
            emitted_anything = 1;
        }

        if (namespace_name[0] != '\0' && !ecsvm_ecsbin_text_buffer_append(&buffer, "}\n")) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
            ecsvm_diagnostic_set(diagnostic, NULL, 0u, 0u, ECSVM_DIAGNOSTIC_OUT_OF_MEMORY, error_message);
            goto fail;
        }
    }

    ecsvm_string_list_free(&namespaces);
    *out_source = buffer.data;
    return ECSVM_OK;

fail:
    ecsvm_string_list_free(&namespaces);
    free(buffer.data);
    return strstr(error_message, "out of memory") != NULL ? ECSVM_ERROR_MEMORY : ECSVM_ERROR_ARGUMENT;
}
