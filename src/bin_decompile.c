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

static const char *ecsvm_ecsbin_token_text(ecsvm_ecsbin_token_kind_t kind)
{
    switch (kind) {
        case ECSVM_ECSBIN_TOKEN_LBRACE: return "{";
        case ECSVM_ECSBIN_TOKEN_RBRACE: return "}";
        case ECSVM_ECSBIN_TOKEN_LBRACKET: return "[";
        case ECSVM_ECSBIN_TOKEN_RBRACKET: return "]";
        case ECSVM_ECSBIN_TOKEN_LPAREN: return "(";
        case ECSVM_ECSBIN_TOKEN_RPAREN: return ")";
        case ECSVM_ECSBIN_TOKEN_COLON: return ":";
        case ECSVM_ECSBIN_TOKEN_SEMICOLON: return ";";
        case ECSVM_ECSBIN_TOKEN_DOT: return ".";
        case ECSVM_ECSBIN_TOKEN_COMMA: return ",";
        case ECSVM_ECSBIN_TOKEN_EQUAL: return "=";
        case ECSVM_ECSBIN_TOKEN_BANG: return "!";
        case ECSVM_ECSBIN_TOKEN_PLUS: return "+";
        case ECSVM_ECSBIN_TOKEN_MINUS: return "-";
        case ECSVM_ECSBIN_TOKEN_STAR: return "*";
        case ECSVM_ECSBIN_TOKEN_SLASH: return "/";
        case ECSVM_ECSBIN_TOKEN_PERCENT: return "%";
        case ECSVM_ECSBIN_TOKEN_LT: return "<";
        case ECSVM_ECSBIN_TOKEN_GT: return ">";
        case ECSVM_ECSBIN_TOKEN_AMPERSAND: return "&";
        case ECSVM_ECSBIN_TOKEN_PIPE: return "|";
        case ECSVM_ECSBIN_TOKEN_CARET: return "^";
        case ECSVM_ECSBIN_TOKEN_TILDE: return "~";
        case ECSVM_ECSBIN_TOKEN_KEY_IMPORT: return "import";
        case ECSVM_ECSBIN_TOKEN_KEY_NAMESPACE: return "namespace";
        case ECSVM_ECSBIN_TOKEN_KEY_STRUCT: return "struct";
        case ECSVM_ECSBIN_TOKEN_KEY_COMPONENT: return "component";
        case ECSVM_ECSBIN_TOKEN_KEY_ATTRIBUTE: return "attribute";
        case ECSVM_ECSBIN_TOKEN_KEY_SYSTEM: return "system";
        case ECSVM_ECSBIN_TOKEN_KEY_CONST: return "const";
        case ECSVM_ECSBIN_TOKEN_KEY_FN: return "fn";
        case ECSVM_ECSBIN_TOKEN_KEY_IF: return "if";
        case ECSVM_ECSBIN_TOKEN_KEY_FOR: return "for";
        case ECSVM_ECSBIN_TOKEN_KEY_IN: return "in";
        case ECSVM_ECSBIN_TOKEN_KEY_ELSE: return "else";
        case ECSVM_ECSBIN_TOKEN_KEY_LET: return "let";
        case ECSVM_ECSBIN_TOKEN_KEY_RETURN: return "return";
        case ECSVM_ECSBIN_TOKEN_KEY_TRUE: return "true";
        case ECSVM_ECSBIN_TOKEN_KEY_FALSE: return "false";
        case ECSVM_ECSBIN_TOKEN_KEY_NULL: return "null";
        case ECSVM_ECSBIN_TOKEN_EOF:
        case ECSVM_ECSBIN_TOKEN_IDENTIFIER:
        case ECSVM_ECSBIN_TOKEN_NUMBER:
        case ECSVM_ECSBIN_TOKEN_STRING:
        default:
            return NULL;
    }
}

static ecsvm_status_t ecsvm_ecsbin_append_ast_token_value(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_ast_node_t *node,
    ecsvm_ecsbin_text_buffer_t *buffer,
    char *error_message,
    size_t error_message_capacity
)
{
    switch (node->value_kind) {
        case ECSVM_ECSBIN_AST_VALUE_NONE: {
            const char *token_text;

            token_text = ecsvm_ecsbin_token_text((ecsvm_ecsbin_token_kind_t)node->token_kind);
            if (token_text == NULL) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function body token text is not available");
                return ECSVM_ERROR_ARGUMENT;
            }
            if (!ecsvm_ecsbin_text_buffer_append(buffer, token_text)) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
                return ECSVM_ERROR_MEMORY;
            }
            return ECSVM_OK;
        }
        case ECSVM_ECSBIN_AST_VALUE_BLOB_ID: {
            const ecsvm_ecsbin_blob_t *blob;

            blob = ecsvm_ecsbin_blob_ref(module, node->value);
            if (blob == NULL) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function body blob reference is invalid");
                return ECSVM_ERROR_ARGUMENT;
            }
            if (!ecsvm_ecsbin_text_buffer_append_range(buffer, (const char *)blob->data, (size_t)blob->length)) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
                return ECSVM_ERROR_MEMORY;
            }
            return ECSVM_OK;
        }
        case ECSVM_ECSBIN_AST_VALUE_TYPE_REF_ID: {
            const ecsvm_ecsbin_type_ref_t *type_ref;

            type_ref = ecsvm_ecsbin_type_ref(module, node->value);
            if (type_ref == NULL || type_ref->name == NULL) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function body type reference is invalid");
                return ECSVM_ERROR_ARGUMENT;
            }
            if (!ecsvm_ecsbin_text_buffer_append(buffer, type_ref->name)) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
                return ECSVM_ERROR_MEMORY;
            }
            return ECSVM_OK;
        }
        case ECSVM_ECSBIN_AST_VALUE_FIELD_REF_ID: {
            const ecsvm_ecsbin_field_ref_t *field_ref;

            if (node->value == 0u || node->value > module->field_ref_count) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function body field reference is invalid");
                return ECSVM_ERROR_ARGUMENT;
            }
            field_ref = &module->field_refs[node->value - 1u];
            if (!ecsvm_ecsbin_text_buffer_append(buffer, field_ref->name)) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
                return ECSVM_ERROR_MEMORY;
            }
            return ECSVM_OK;
        }
        case ECSVM_ECSBIN_AST_VALUE_FUNCTION_REF_ID: {
            const ecsvm_ecsbin_function_ref_t *function_ref;

            if (node->value == 0u || node->value > module->function_ref_count) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function body function reference is invalid");
                return ECSVM_ERROR_ARGUMENT;
            }
            function_ref = &module->function_refs[node->value - 1u];
            if (!ecsvm_ecsbin_text_buffer_append(buffer, function_ref->name)) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
                return ECSVM_ERROR_MEMORY;
            }
            return ECSVM_OK;
        }
        case ECSVM_ECSBIN_AST_VALUE_PARAMETER_ID: {
            const ecsvm_ecsbin_parameter_t *parameter;

            parameter = ecsvm_ecsbin_parameter_ref(module, node->value);
            if (parameter == NULL || parameter->name == NULL) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function body parameter reference is invalid");
                return ECSVM_ERROR_ARGUMENT;
            }
            if (!ecsvm_ecsbin_text_buffer_append(buffer, parameter->name)) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
                return ECSVM_ERROR_MEMORY;
            }
            return ECSVM_OK;
        }
        default:
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function body token value kind is not supported");
            return ECSVM_ERROR_ARGUMENT;
    }
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
    if (version != ECSVM_ECSBIN_AST_VERSION_2 &&
        version != ECSVM_ECSBIN_AST_VERSION_3) {
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
    out_ast->version = version;
    return ECSVM_OK;
}

static ecsvm_status_t ecsvm_ecsbin_render_ast_inline(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_ast_blob_t *ast,
    uint32_t child_index,
    ecsvm_ecsbin_text_buffer_t *buffer,
    char *error_message,
    size_t error_message_capacity
);

static ecsvm_status_t ecsvm_ecsbin_render_ast_inline_node(
    const ecsvm_ecsbin_module_t *module,
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

static int ecsvm_ecsbin_nodes_need_no_space_between(
    const ecsvm_ecsbin_ast_node_t *previous,
    const ecsvm_ecsbin_ast_node_t *current
)
{
    if (previous == NULL || current == NULL) {
        return 0;
    }

    if (current->kind == ECSVM_ECSBIN_AST_NODE_GROUP_PAREN) {
        return (previous->kind == ECSVM_ECSBIN_AST_NODE_TOKEN &&
                previous->token_kind == ECSVM_ECSBIN_TOKEN_IDENTIFIER) ||
            previous->kind == ECSVM_ECSBIN_AST_NODE_GROUP_PAREN ||
            previous->kind == ECSVM_ECSBIN_AST_NODE_GROUP_BRACKET;
    }

    return 0;
}

static ecsvm_status_t ecsvm_ecsbin_render_ast_block(
    const ecsvm_ecsbin_module_t *module,
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
            module,
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
                    module,
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
                !ecsvm_ecsbin_nodes_need_no_space_between(previous, child) &&
                !ecsvm_ecsbin_node_needs_no_leading_space(child) &&
                !ecsvm_ecsbin_node_needs_no_trailing_space(previous) &&
                !ecsvm_ecsbin_text_buffer_append_char(buffer, ' ')) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
                return ECSVM_ERROR_MEMORY;
            }

            status = ecsvm_ecsbin_render_ast_inline_node(
                module,
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
    const ecsvm_ecsbin_module_t *module,
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
        return ecsvm_ecsbin_append_ast_token_value(module, node, buffer, error_message, error_message_capacity);
    }

    if (node->kind == ECSVM_ECSBIN_AST_NODE_GROUP_PAREN) {
        if (!ecsvm_ecsbin_text_buffer_append_char(buffer, '(')) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
            return ECSVM_ERROR_MEMORY;
        }
        if (node->first_child != 0u) {
            ecsvm_status_t status;

            status = ecsvm_ecsbin_render_ast_inline(
                module,
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
                module,
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
            module,
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
    const ecsvm_ecsbin_module_t *module,
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
            !ecsvm_ecsbin_nodes_need_no_space_between(previous, node) &&
            !ecsvm_ecsbin_node_needs_no_leading_space(node) &&
            !ecsvm_ecsbin_node_needs_no_trailing_space(previous) &&
            !ecsvm_ecsbin_text_buffer_append_char(buffer, ' ')) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
            return ECSVM_ERROR_MEMORY;
        }

        status = ecsvm_ecsbin_render_ast_inline_node(
            module,
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

static ecsvm_status_t ecsvm_ecsbin_render_ast_v3_expression(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_ast_blob_t *ast,
    uint32_t node_index,
    ecsvm_ecsbin_text_buffer_t *buffer,
    char *error_message,
    size_t error_message_capacity
);

static ecsvm_status_t ecsvm_ecsbin_render_ast_v3_statement(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_ast_blob_t *ast,
    uint32_t node_index,
    size_t indent,
    ecsvm_ecsbin_text_buffer_t *buffer,
    char *error_message,
    size_t error_message_capacity
);

static ecsvm_status_t ecsvm_ecsbin_render_ast_v3_value(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_ast_node_t *node,
    ecsvm_ecsbin_text_buffer_t *buffer,
    char *error_message,
    size_t error_message_capacity
)
{
    if (node->value_kind == ECSVM_ECSBIN_AST_VALUE_NONE) {
        return ECSVM_OK;
    }

    return ecsvm_ecsbin_append_ast_token_value(
        module,
        node,
        buffer,
        error_message,
        error_message_capacity
    );
}

static ecsvm_status_t ecsvm_ecsbin_render_ast_v3_expression_list(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_ast_blob_t *ast,
    uint32_t child_index,
    ecsvm_ecsbin_text_buffer_t *buffer,
    char *error_message,
    size_t error_message_capacity
)
{
    int first;

    first = 1;
    while (child_index != 0u) {
        const ecsvm_ecsbin_ast_node_t *node;
        ecsvm_status_t status;

        node = ecsvm_ecsbin_ast_node(ast, child_index, error_message, error_message_capacity);
        if (node == NULL) {
            return ECSVM_ERROR_ARGUMENT;
        }

        if (!first && !ecsvm_ecsbin_text_buffer_append(buffer, ", ")) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling function body");
            return ECSVM_ERROR_MEMORY;
        }

        status = ecsvm_ecsbin_render_ast_v3_expression(
            module,
            ast,
            child_index,
            buffer,
            error_message,
            error_message_capacity
        );
        if (status != ECSVM_OK) {
            return status;
        }

        first = 0;
        child_index = node->next_sibling;
    }

    return ECSVM_OK;
}

static ecsvm_status_t ecsvm_ecsbin_render_ast_v3_expression(
    const ecsvm_ecsbin_module_t *module,
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

    switch (node->kind) {
        case ECSVM_ECSBIN_AST_NODE_IDENTIFIER:
        case ECSVM_ECSBIN_AST_NODE_TYPE_EXPRESSION:
        case ECSVM_ECSBIN_AST_NODE_LITERAL_EXPRESSION:
            return ecsvm_ecsbin_render_ast_v3_value(module, node, buffer, error_message, error_message_capacity);
        case ECSVM_ECSBIN_AST_NODE_GROUPING_EXPRESSION: {
            ecsvm_status_t status;
            if (!ecsvm_ecsbin_text_buffer_append_char(buffer, '(')) {
                return ECSVM_ERROR_MEMORY;
            }
            status = ecsvm_ecsbin_render_ast_v3_expression(
                module,
                ast,
                node->first_child,
                buffer,
                error_message,
                error_message_capacity
            );
            if (status != ECSVM_OK) {
                return status;
            }
            if (!ecsvm_ecsbin_text_buffer_append_char(buffer, ')')) {
                return ECSVM_ERROR_MEMORY;
            }
            return ECSVM_OK;
        }
        case ECSVM_ECSBIN_AST_NODE_ARGUMENT_LIST:
            return ecsvm_ecsbin_render_ast_v3_expression_list(
                module,
                ast,
                node->first_child,
                buffer,
                error_message,
                error_message_capacity
            );
        case ECSVM_ECSBIN_AST_NODE_CALL_EXPRESSION: {
            const ecsvm_ecsbin_ast_node_t *callee;
            const ecsvm_ecsbin_ast_node_t *arguments;
            ecsvm_status_t status;

            callee = node->first_child == 0u ? NULL : ecsvm_ecsbin_ast_node(ast, node->first_child, error_message, error_message_capacity);
            arguments = (callee != NULL && callee->next_sibling != 0u)
                ? ecsvm_ecsbin_ast_node(ast, callee->next_sibling, error_message, error_message_capacity)
                : NULL;
            if (callee == NULL || arguments == NULL) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "call expression is malformed");
                return ECSVM_ERROR_ARGUMENT;
            }

            status = ecsvm_ecsbin_render_ast_v3_expression(
                module,
                ast,
                node->first_child,
                buffer,
                error_message,
                error_message_capacity
            );
            if (status != ECSVM_OK) {
                return status;
            }
            if (!ecsvm_ecsbin_text_buffer_append_char(buffer, '(')) {
                return ECSVM_ERROR_MEMORY;
            }
            status = ecsvm_ecsbin_render_ast_v3_expression_list(
                module,
                ast,
                arguments->first_child,
                buffer,
                error_message,
                error_message_capacity
            );
            if (status != ECSVM_OK) {
                return status;
            }
            if (!ecsvm_ecsbin_text_buffer_append_char(buffer, ')')) {
                return ECSVM_ERROR_MEMORY;
            }
            return ECSVM_OK;
        }
        case ECSVM_ECSBIN_AST_NODE_OBJECT_LITERAL: {
            ecsvm_status_t status;
            uint32_t child_index;
            int first;

            if (!ecsvm_ecsbin_text_buffer_append_char(buffer, '{')) {
                return ECSVM_ERROR_MEMORY;
            }

            child_index = node->first_child;
            first = 1;
            while (child_index != 0u) {
                if (!first &&
                    !ecsvm_ecsbin_text_buffer_append(buffer, ", ")) {
                    return ECSVM_ERROR_MEMORY;
                }
                status = ecsvm_ecsbin_render_ast_v3_expression(
                    module,
                    ast,
                    child_index,
                    buffer,
                    error_message,
                    error_message_capacity
                );
                if (status != ECSVM_OK) {
                    return status;
                }
                first = 0;
                child_index = ast->nodes[child_index].next_sibling;
            }

            return ecsvm_ecsbin_text_buffer_append_char(buffer, '}')
                ? ECSVM_OK
                : ECSVM_ERROR_MEMORY;
        }
        case ECSVM_ECSBIN_AST_NODE_OBJECT_FIELD: {
            ecsvm_status_t status;
            const ecsvm_ecsbin_ast_node_t *value_node;

            value_node = node->first_child == 0u
                ? NULL
                : ecsvm_ecsbin_ast_node(ast, node->first_child, error_message, error_message_capacity);
            if (value_node == NULL) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "object field is malformed");
                return ECSVM_ERROR_ARGUMENT;
            }

            status = ecsvm_ecsbin_render_ast_v3_value(module, node, buffer, error_message, error_message_capacity);
            if (status != ECSVM_OK) {
                return status;
            }
            if (!ecsvm_ecsbin_text_buffer_append(buffer, ": ")) {
                return ECSVM_ERROR_MEMORY;
            }
            return ecsvm_ecsbin_render_ast_v3_expression(
                module,
                ast,
                node->first_child,
                buffer,
                error_message,
                error_message_capacity
            );
        }
        case ECSVM_ECSBIN_AST_NODE_MEMBER_EXPRESSION:
        case ECSVM_ECSBIN_AST_NODE_INDEX_EXPRESSION:
        case ECSVM_ECSBIN_AST_NODE_ASSIGNMENT_EXPRESSION:
        case ECSVM_ECSBIN_AST_NODE_BINARY_EXPRESSION: {
            const ecsvm_ecsbin_ast_node_t *left;
            const ecsvm_ecsbin_ast_node_t *right;
            ecsvm_status_t status;

            left = node->first_child == 0u ? NULL : ecsvm_ecsbin_ast_node(ast, node->first_child, error_message, error_message_capacity);
            right = (left != NULL && left->next_sibling != 0u)
                ? ecsvm_ecsbin_ast_node(ast, left->next_sibling, error_message, error_message_capacity)
                : NULL;
            if (left == NULL || right == NULL) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "binary expression is malformed");
                return ECSVM_ERROR_ARGUMENT;
            }

            status = ecsvm_ecsbin_render_ast_v3_expression(
                module,
                ast,
                node->first_child,
                buffer,
                error_message,
                error_message_capacity
            );
            if (status != ECSVM_OK) {
                return status;
            }

            if (node->kind == ECSVM_ECSBIN_AST_NODE_MEMBER_EXPRESSION) {
                if (!ecsvm_ecsbin_text_buffer_append_char(buffer, '.')) {
                    return ECSVM_ERROR_MEMORY;
                }
            } else if (node->kind == ECSVM_ECSBIN_AST_NODE_INDEX_EXPRESSION) {
                if (!ecsvm_ecsbin_text_buffer_append_char(buffer, '[')) {
                    return ECSVM_ERROR_MEMORY;
                }
            } else {
                if (!ecsvm_ecsbin_text_buffer_append_char(buffer, ' ')) {
                    return ECSVM_ERROR_MEMORY;
                }
                status = ecsvm_ecsbin_render_ast_v3_value(module, node, buffer, error_message, error_message_capacity);
                if (status != ECSVM_OK) {
                    return status;
                }
                if (!ecsvm_ecsbin_text_buffer_append_char(buffer, ' ')) {
                    return ECSVM_ERROR_MEMORY;
                }
            }

            status = ecsvm_ecsbin_render_ast_v3_expression(
                module,
                ast,
                left->next_sibling,
                buffer,
                error_message,
                error_message_capacity
            );
            if (status != ECSVM_OK) {
                return status;
            }

            if (node->kind == ECSVM_ECSBIN_AST_NODE_INDEX_EXPRESSION &&
                !ecsvm_ecsbin_text_buffer_append_char(buffer, ']')) {
                return ECSVM_ERROR_MEMORY;
            }

            return ECSVM_OK;
        }
        case ECSVM_ECSBIN_AST_NODE_UNARY_EXPRESSION: {
            ecsvm_status_t status;
            status = ecsvm_ecsbin_render_ast_v3_value(module, node, buffer, error_message, error_message_capacity);
            if (status != ECSVM_OK) {
                return status;
            }
            return ecsvm_ecsbin_render_ast_v3_expression(
                module,
                ast,
                node->first_child,
                buffer,
                error_message,
                error_message_capacity
            );
        }
        default:
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "unsupported ast expression node");
            return ECSVM_ERROR_ARGUMENT;
    }
}

static ecsvm_status_t ecsvm_ecsbin_render_ast_v3_block(
    const ecsvm_ecsbin_module_t *module,
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
    if (node == NULL || node->kind != ECSVM_ECSBIN_AST_NODE_BLOCK) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "expected block ast node");
        return ECSVM_ERROR_ARGUMENT;
    }

    if (!ecsvm_ecsbin_text_buffer_append_char(buffer, '{') ||
        !ecsvm_ecsbin_text_buffer_append_char(buffer, '\n')) {
        return ECSVM_ERROR_MEMORY;
    }

    child_index = node->first_child;
    while (child_index != 0u) {
        ecsvm_status_t status;
        status = ecsvm_ecsbin_render_ast_v3_statement(
            module,
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
        child_index = ast->nodes[child_index].next_sibling;
    }

    if (!ecsvm_ecsbin_text_buffer_append_indent(buffer, indent) ||
        !ecsvm_ecsbin_text_buffer_append_char(buffer, '}')) {
        return ECSVM_ERROR_MEMORY;
    }

    return ECSVM_OK;
}

static ecsvm_status_t ecsvm_ecsbin_render_ast_v3_statement(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_ast_blob_t *ast,
    uint32_t node_index,
    size_t indent,
    ecsvm_ecsbin_text_buffer_t *buffer,
    char *error_message,
    size_t error_message_capacity
)
{
    const ecsvm_ecsbin_ast_node_t *node;
    ecsvm_status_t status;

    node = ecsvm_ecsbin_ast_node(ast, node_index, error_message, error_message_capacity);
    if (node == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    if (!ecsvm_ecsbin_text_buffer_append_indent(buffer, indent)) {
        return ECSVM_ERROR_MEMORY;
    }

    switch (node->kind) {
        case ECSVM_ECSBIN_AST_NODE_BLOCK:
            status = ecsvm_ecsbin_render_ast_v3_block(module, ast, node_index, indent, buffer, error_message, error_message_capacity);
            if (status != ECSVM_OK) {
                return status;
            }
            return ecsvm_ecsbin_text_buffer_append_char(buffer, '\n') ? ECSVM_OK : ECSVM_ERROR_MEMORY;
        case ECSVM_ECSBIN_AST_NODE_DECLARATION: {
            const ecsvm_ecsbin_ast_node_t *name_node;
            const ecsvm_ecsbin_ast_node_t *type_node;
            const ecsvm_ecsbin_ast_node_t *value_node;

            name_node = node->first_child == 0u ? NULL : ecsvm_ecsbin_ast_node(ast, node->first_child, error_message, error_message_capacity);
            type_node = (name_node != NULL && name_node->next_sibling != 0u)
                ? ecsvm_ecsbin_ast_node(ast, name_node->next_sibling, error_message, error_message_capacity)
                : NULL;
            value_node = type_node;
            if (type_node != NULL && type_node->kind == ECSVM_ECSBIN_AST_NODE_TYPE_EXPRESSION) {
                value_node = type_node->next_sibling == 0u
                    ? NULL
                    : ecsvm_ecsbin_ast_node(ast, type_node->next_sibling, error_message, error_message_capacity);
            } else {
                type_node = NULL;
            }

            if (name_node == NULL ||
                !ecsvm_ecsbin_text_buffer_append(
                    buffer,
                    node->token_kind == ECSVM_ECSBIN_TOKEN_KEY_CONST ? "const " : "let "
                )) {
                return ECSVM_ERROR_MEMORY;
            }
            status = ecsvm_ecsbin_render_ast_v3_expression(module, ast, node->first_child, buffer, error_message, error_message_capacity);
            if (status != ECSVM_OK) {
                return status;
            }
            if (type_node != NULL) {
                if (!ecsvm_ecsbin_text_buffer_append(buffer, ": ")) {
                    return ECSVM_ERROR_MEMORY;
                }
                status = ecsvm_ecsbin_render_ast_v3_expression(module, ast, name_node->next_sibling, buffer, error_message, error_message_capacity);
                if (status != ECSVM_OK) {
                    return status;
                }
            }
            if (value_node != NULL) {
                if (!ecsvm_ecsbin_text_buffer_append(buffer, " = ")) {
                    return ECSVM_ERROR_MEMORY;
                }
                status = ecsvm_ecsbin_render_ast_v3_expression(module, ast, type_node != NULL ? type_node->next_sibling : name_node->next_sibling, buffer, error_message, error_message_capacity);
                if (status != ECSVM_OK) {
                    return status;
                }
            }
            if (!ecsvm_ecsbin_text_buffer_append(buffer, ";\n")) {
                return ECSVM_ERROR_MEMORY;
            }
            return ECSVM_OK;
        }
        case ECSVM_ECSBIN_AST_NODE_RETURN_STATEMENT:
            if (!ecsvm_ecsbin_text_buffer_append(buffer, "return")) {
                return ECSVM_ERROR_MEMORY;
            }
            if (node->first_child != 0u) {
                if (!ecsvm_ecsbin_text_buffer_append_char(buffer, ' ')) {
                    return ECSVM_ERROR_MEMORY;
                }
                status = ecsvm_ecsbin_render_ast_v3_expression(module, ast, node->first_child, buffer, error_message, error_message_capacity);
                if (status != ECSVM_OK) {
                    return status;
                }
            }
            return ecsvm_ecsbin_text_buffer_append(buffer, ";\n") ? ECSVM_OK : ECSVM_ERROR_MEMORY;
        case ECSVM_ECSBIN_AST_NODE_EXPRESSION_STATEMENT:
            status = ecsvm_ecsbin_render_ast_v3_expression(module, ast, node->first_child, buffer, error_message, error_message_capacity);
            if (status != ECSVM_OK) {
                return status;
            }
            return ecsvm_ecsbin_text_buffer_append(buffer, ";\n") ? ECSVM_OK : ECSVM_ERROR_MEMORY;
        case ECSVM_ECSBIN_AST_NODE_IF_STATEMENT: {
            const ecsvm_ecsbin_ast_node_t *condition_node;
            const ecsvm_ecsbin_ast_node_t *then_node;
            const ecsvm_ecsbin_ast_node_t *else_node;

            condition_node = node->first_child == 0u ? NULL : ecsvm_ecsbin_ast_node(ast, node->first_child, error_message, error_message_capacity);
            then_node = (condition_node != NULL && condition_node->next_sibling != 0u)
                ? ecsvm_ecsbin_ast_node(ast, condition_node->next_sibling, error_message, error_message_capacity)
                : NULL;
            else_node = (then_node != NULL && then_node->next_sibling != 0u)
                ? ecsvm_ecsbin_ast_node(ast, then_node->next_sibling, error_message, error_message_capacity)
                : NULL;
            if (condition_node == NULL || then_node == NULL ||
                !ecsvm_ecsbin_text_buffer_append(buffer, "if (")) {
                return ECSVM_ERROR_MEMORY;
            }
            status = ecsvm_ecsbin_render_ast_v3_expression(module, ast, node->first_child, buffer, error_message, error_message_capacity);
            if (status != ECSVM_OK) {
                return status;
            }
            if (!ecsvm_ecsbin_text_buffer_append(buffer, ") ")) {
                return ECSVM_ERROR_MEMORY;
            }
            if (then_node->kind == ECSVM_ECSBIN_AST_NODE_BLOCK) {
                status = ecsvm_ecsbin_render_ast_v3_block(module, ast, condition_node->next_sibling, indent, buffer, error_message, error_message_capacity);
                if (status != ECSVM_OK) {
                    return status;
                }
            } else {
                if (!ecsvm_ecsbin_text_buffer_append_char(buffer, '\n')) {
                    return ECSVM_ERROR_MEMORY;
                }
                status = ecsvm_ecsbin_render_ast_v3_statement(module, ast, condition_node->next_sibling, indent + 1u, buffer, error_message, error_message_capacity);
                if (status != ECSVM_OK) {
                    return status;
                }
                if (!ecsvm_ecsbin_text_buffer_append_indent(buffer, indent)) {
                    return ECSVM_ERROR_MEMORY;
                }
            }
            if (else_node != NULL) {
                if (!ecsvm_ecsbin_text_buffer_append(buffer, " else ")) {
                    return ECSVM_ERROR_MEMORY;
                }
                status = ecsvm_ecsbin_render_ast_v3_statement(module, ast, then_node->next_sibling, indent, buffer, error_message, error_message_capacity);
                if (status != ECSVM_OK) {
                    return status;
                }
                return ECSVM_OK;
            }
            return ecsvm_ecsbin_text_buffer_append_char(buffer, '\n') ? ECSVM_OK : ECSVM_ERROR_MEMORY;
        }
        case ECSVM_ECSBIN_AST_NODE_FOR_IN_STATEMENT: {
            const ecsvm_ecsbin_ast_node_t *name_node;
            const ecsvm_ecsbin_ast_node_t *type_node;
            const ecsvm_ecsbin_ast_node_t *body_node;

            name_node = node->first_child == 0u ? NULL : ecsvm_ecsbin_ast_node(ast, node->first_child, error_message, error_message_capacity);
            type_node = (name_node != NULL && name_node->next_sibling != 0u)
                ? ecsvm_ecsbin_ast_node(ast, name_node->next_sibling, error_message, error_message_capacity)
                : NULL;
            body_node = (type_node != NULL && type_node->next_sibling != 0u)
                ? ecsvm_ecsbin_ast_node(ast, type_node->next_sibling, error_message, error_message_capacity)
                : NULL;
            if (name_node == NULL ||
                type_node == NULL ||
                body_node == NULL ||
                !ecsvm_ecsbin_text_buffer_append(buffer, "for (")) {
                return ECSVM_ERROR_MEMORY;
            }
            status = ecsvm_ecsbin_render_ast_v3_expression(module, ast, node->first_child, buffer, error_message, error_message_capacity);
            if (status != ECSVM_OK) {
                return status;
            }
            if (!ecsvm_ecsbin_text_buffer_append(buffer, " in getEntities(")) {
                return ECSVM_ERROR_MEMORY;
            }
            status = ecsvm_ecsbin_render_ast_v3_expression(module, ast, name_node->next_sibling, buffer, error_message, error_message_capacity);
            if (status != ECSVM_OK) {
                return status;
            }
            if (!ecsvm_ecsbin_text_buffer_append(buffer, ")) ")) {
                return ECSVM_ERROR_MEMORY;
            }
            if (body_node->kind == ECSVM_ECSBIN_AST_NODE_BLOCK) {
                status = ecsvm_ecsbin_render_ast_v3_block(module, ast, type_node->next_sibling, indent, buffer, error_message, error_message_capacity);
                if (status != ECSVM_OK) {
                    return status;
                }
                return ecsvm_ecsbin_text_buffer_append_char(buffer, '\n') ? ECSVM_OK : ECSVM_ERROR_MEMORY;
            }
            if (!ecsvm_ecsbin_text_buffer_append_char(buffer, '\n')) {
                return ECSVM_ERROR_MEMORY;
            }
            return ecsvm_ecsbin_render_ast_v3_statement(module, ast, type_node->next_sibling, indent + 1u, buffer, error_message, error_message_capacity);
        }
        case ECSVM_ECSBIN_AST_NODE_ELSE_CLAUSE:
            if (node->first_child == 0u) {
                return ECSVM_OK;
            }
            return ecsvm_ecsbin_render_ast_v3_statement(module, ast, node->first_child, indent, buffer, error_message, error_message_capacity);
        default:
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "unsupported ast statement node");
            return ECSVM_ERROR_ARGUMENT;
    }
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
    if (ast.version == ECSVM_ECSBIN_AST_VERSION_2) {
        status = ecsvm_ecsbin_render_ast_block(
            module,
            &ast,
            ast.nodes[0].first_child,
            0u,
            &buffer,
            error_message,
            error_message_capacity
        );
    } else {
        status = ecsvm_ecsbin_render_ast_v3_block(
            module,
            &ast,
            ast.nodes[0].first_child,
            0u,
            &buffer,
            error_message,
            error_message_capacity
        );
    }
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
    if (strcmp(qualified_name, "core.UInt64") == 0) {
        return "u64";
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
    int is_attribute;
    int is_component;

    definition = &module->struct_defs[struct_index];
    type_ref = ecsvm_ecsbin_type_ref(module, definition->type_id);
    if (type_ref == NULL) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "struct type reference is invalid");
        return ECSVM_ERROR_ARGUMENT;
    }

    is_component = ecsvm_ecsbin_struct_is_component(module, definition);
    is_attribute = 0;
    for (attribute_index = 0u; attribute_index < definition->attribute_count; ++attribute_index) {
        const ecsvm_ecsbin_attribute_t *attribute;
        const ecsvm_ecsbin_type_ref_t *attribute_type;

        attribute = ecsvm_ecsbin_attribute_ref(module, definition->attribute_start + (uint32_t)attribute_index);
        attribute_type = attribute != NULL ? ecsvm_ecsbin_type_ref(module, attribute->type_id) : NULL;
        if (attribute_type == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "struct attribute is invalid");
            return ECSVM_ERROR_ARGUMENT;
        }
        if (strcmp(attribute_type->qualified_name, "core.Attribute") == 0) {
            is_attribute = 1;
        }
        if (is_component && strcmp(attribute_type->qualified_name, "core.Component") == 0) {
            continue;
        }
        if (!is_component &&
            is_attribute &&
            strcmp(attribute_type->qualified_name, "core.Attribute") == 0) {
            continue;
        }
        if (!ecsvm_ecsbin_text_buffer_append(buffer, indent) ||
            !ecsvm_ecsbin_text_buffer_append_char(buffer, '[') ||
            !ecsvm_ecsbin_text_buffer_append(buffer, ecsvm_display_type_name(attribute_type, current_namespace))) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
            return ECSVM_ERROR_MEMORY;
        }
        if (ecsvm_ecsbin_attribute_expects_type_payload(module, attribute)) {
            uint32_t payload_type_id;
            const ecsvm_ecsbin_type_ref_t *payload_type;

            if (!ecsvm_ecsbin_attribute_type_payload(module, attribute, &payload_type_id)) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "attribute payload is invalid");
                return ECSVM_ERROR_ARGUMENT;
            }

            payload_type = ecsvm_ecsbin_type_ref(module, payload_type_id);
            if (payload_type == NULL ||
                payload_type->qualified_name == NULL ||
                !ecsvm_ecsbin_text_buffer_append_char(buffer, '(') ||
                !ecsvm_ecsbin_text_buffer_append(buffer, ecsvm_display_type_name(payload_type, current_namespace)) ||
                !ecsvm_ecsbin_text_buffer_append_char(buffer, ')')) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
                return ECSVM_ERROR_MEMORY;
            }
        } else if (attribute->data != NULL &&
                   attribute->data[0] != '\0' &&
                   (!ecsvm_ecsbin_text_buffer_append_char(buffer, '(') ||
                    !ecsvm_ecsbin_text_buffer_append(buffer, attribute->data) ||
                    !ecsvm_ecsbin_text_buffer_append_char(buffer, ')'))) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
            return ECSVM_ERROR_MEMORY;
        }
        if (!ecsvm_ecsbin_text_buffer_append(buffer, "]\n")) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
            return ECSVM_ERROR_MEMORY;
        }
    }

    if (definition->field_count == 0u) {
        const char *kind_text;

        kind_text = is_component ? "component " : (is_attribute ? "attribute " : "struct ");
        if (!ecsvm_ecsbin_text_buffer_append(buffer, indent) ||
            !ecsvm_ecsbin_text_buffer_append(buffer, kind_text) ||
            !ecsvm_ecsbin_text_buffer_append(buffer, type_ref->name) ||
            !ecsvm_ecsbin_text_buffer_append(buffer, ";\n")) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
            return ECSVM_ERROR_MEMORY;
        }
        return ECSVM_OK;
    }

    if (!ecsvm_ecsbin_text_buffer_append(buffer, indent) ||
        !ecsvm_ecsbin_text_buffer_append(buffer, is_component ? "component " : (is_attribute ? "attribute " : "struct ")) ||
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
    size_t attribute_index;
    size_t parameter_index;
    int is_system;

    function_ref = &module->function_refs[function_index];
    return_type = ecsvm_ecsbin_function_return_type(module, function_ref);
    is_system = ecsvm_ecsbin_function_has_attribute(module, function_ref, "core.System");
    if (return_type == NULL) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function return type is invalid");
        return ECSVM_ERROR_ARGUMENT;
    }

    for (attribute_index = 1u; attribute_index < function_ref->attribute_count; ++attribute_index) {
        const ecsvm_ecsbin_attribute_t *attribute;
        const ecsvm_ecsbin_type_ref_t *attribute_type;

        attribute = ecsvm_ecsbin_attribute_ref(module, function_ref->attribute_start + (uint32_t)attribute_index);
        attribute_type = attribute != NULL ? ecsvm_ecsbin_type_ref(module, attribute->type_id) : NULL;
        if (attribute_type == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function attribute is invalid");
            return ECSVM_ERROR_ARGUMENT;
        }
        if (strcmp(attribute_type->qualified_name, "core.System") == 0) {
            continue;
        }
        if (!ecsvm_ecsbin_text_buffer_append(buffer, indent) ||
            !ecsvm_ecsbin_text_buffer_append_char(buffer, '[') ||
            !ecsvm_ecsbin_text_buffer_append(buffer, ecsvm_display_type_name(attribute_type, current_namespace))) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
            return ECSVM_ERROR_MEMORY;
        }
        if (ecsvm_ecsbin_attribute_expects_type_payload(module, attribute)) {
            uint32_t payload_type_id;
            const ecsvm_ecsbin_type_ref_t *payload_type;

            if (!ecsvm_ecsbin_attribute_type_payload(module, attribute, &payload_type_id)) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "function attribute payload is invalid");
                return ECSVM_ERROR_ARGUMENT;
            }

            payload_type = ecsvm_ecsbin_type_ref(module, payload_type_id);
            if (payload_type == NULL ||
                payload_type->qualified_name == NULL ||
                !ecsvm_ecsbin_text_buffer_append_char(buffer, '(') ||
                !ecsvm_ecsbin_text_buffer_append(buffer, ecsvm_display_type_name(payload_type, current_namespace)) ||
                !ecsvm_ecsbin_text_buffer_append_char(buffer, ')')) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
                return ECSVM_ERROR_MEMORY;
            }
        } else if (attribute->data != NULL && attribute->data[0] != '\0') {
            if (!ecsvm_ecsbin_text_buffer_append_char(buffer, '(') ||
                !ecsvm_ecsbin_text_buffer_append(buffer, attribute->data) ||
                !ecsvm_ecsbin_text_buffer_append_char(buffer, ')')) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
                return ECSVM_ERROR_MEMORY;
            }
        }
        if (!ecsvm_ecsbin_text_buffer_append(buffer, "]\n")) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while decompiling module");
            return ECSVM_ERROR_MEMORY;
        }
    }

    if (!ecsvm_ecsbin_text_buffer_append(buffer, indent) ||
        !ecsvm_ecsbin_text_buffer_append(buffer, is_system ? "system " : "fn ") ||
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
    if (!is_system &&
        strcmp(return_type->qualified_name, "core.Void") != 0 &&
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
