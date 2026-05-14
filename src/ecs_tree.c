#include "ecs_tree.h"

static const char *ecsvm_ecs_tree_syntax_kind_string(ecsvm_syntax_kind_t kind)
{
    switch (kind) {
    case ECSVM_SYNTAX_ROOT:
        return "root";
    case ECSVM_SYNTAX_FILE:
        return "document";
    case ECSVM_SYNTAX_IMPORT:
        return "import";
    case ECSVM_SYNTAX_NAMESPACE:
        return "namespace";
    case ECSVM_SYNTAX_STRUCT:
        return "struct";
    case ECSVM_SYNTAX_FIELD:
        return "field";
    case ECSVM_SYNTAX_ATTRIBUTE:
        return "attribute";
    case ECSVM_SYNTAX_FUNCTION:
        return "function";
    case ECSVM_SYNTAX_PARAMETER:
        return "parameter";
    }

    return "unknown";
}

static const char *ecsvm_ecs_tree_token_kind_string(ecsvm_token_kind_t kind)
{
    switch (kind) {
    case ECSVM_TOKEN_EOF:
        return "eof";
    case ECSVM_TOKEN_IDENTIFIER:
        return "identifier";
    case ECSVM_TOKEN_NUMBER:
        return "number";
    case ECSVM_TOKEN_STRING:
        return "string";
    case ECSVM_TOKEN_LBRACE:
        return "lbrace";
    case ECSVM_TOKEN_RBRACE:
        return "rbrace";
    case ECSVM_TOKEN_LBRACKET:
        return "lbracket";
    case ECSVM_TOKEN_RBRACKET:
        return "rbracket";
    case ECSVM_TOKEN_LPAREN:
        return "lparen";
    case ECSVM_TOKEN_RPAREN:
        return "rparen";
    case ECSVM_TOKEN_COLON:
        return "colon";
    case ECSVM_TOKEN_SEMICOLON:
        return "semicolon";
    case ECSVM_TOKEN_DOT:
        return "dot";
    case ECSVM_TOKEN_COMMA:
        return "comma";
    case ECSVM_TOKEN_EQUAL:
        return "equal";
    case ECSVM_TOKEN_BANG:
        return "bang";
    case ECSVM_TOKEN_PLUS:
        return "plus";
    case ECSVM_TOKEN_MINUS:
        return "minus";
    case ECSVM_TOKEN_STAR:
        return "star";
    case ECSVM_TOKEN_SLASH:
        return "slash";
    case ECSVM_TOKEN_PERCENT:
        return "percent";
    case ECSVM_TOKEN_LT:
        return "lt";
    case ECSVM_TOKEN_GT:
        return "gt";
    case ECSVM_TOKEN_AMPERSAND:
        return "ampersand";
    case ECSVM_TOKEN_PIPE:
        return "pipe";
    case ECSVM_TOKEN_CARET:
        return "caret";
    case ECSVM_TOKEN_TILDE:
        return "tilde";
    case ECSVM_TOKEN_KEY_IMPORT:
        return "keyword_import";
    case ECSVM_TOKEN_KEY_NAMESPACE:
        return "keyword_namespace";
    case ECSVM_TOKEN_KEY_STRUCT:
        return "keyword_struct";
    case ECSVM_TOKEN_KEY_COMPONENT:
        return "keyword_component";
    case ECSVM_TOKEN_KEY_ATTRIBUTE:
        return "keyword_attribute";
    case ECSVM_TOKEN_KEY_SYSTEM:
        return "keyword_system";
    case ECSVM_TOKEN_KEY_CONST:
        return "keyword_const";
    case ECSVM_TOKEN_KEY_FN:
        return "keyword_fn";
    case ECSVM_TOKEN_KEY_IF:
        return "keyword_if";
    case ECSVM_TOKEN_KEY_ELSE:
        return "keyword_else";
    case ECSVM_TOKEN_KEY_LET:
        return "keyword_let";
    case ECSVM_TOKEN_KEY_RETURN:
        return "keyword_return";
    case ECSVM_TOKEN_KEY_TRUE:
        return "keyword_true";
    case ECSVM_TOKEN_KEY_FALSE:
        return "keyword_false";
    case ECSVM_TOKEN_KEY_NULL:
        return "keyword_null";
    }

    return "unknown";
}

static int ecsvm_ecs_tree_node_is_synthetic(const ecsvm_syntax_node_t *node)
{
    return node->kind == ECSVM_SYNTAX_ATTRIBUTE &&
        node->token_start == 0u &&
        node->token_end == 0u &&
        node->name_start == 0u &&
        node->name_end == 0u &&
        (node->is_component || node->is_attribute);
}

static int ecsvm_ecs_tree_node_range(
    const ecsvm_source_file_t *source_file,
    size_t node_index,
    size_t *out_start,
    size_t *out_end
)
{
    const ecsvm_syntax_node_t *node;

    if (source_file == NULL || node_index >= source_file->nodes.count) {
        return 0;
    }

    node = &source_file->nodes.items[node_index];
    if (ecsvm_ecs_tree_node_is_synthetic(node)) {
        return 0;
    }

    if (node->kind == ECSVM_SYNTAX_FILE) {
        if (source_file->tokens.count <= 1u) {
            return 0;
        }
        *out_start = 0u;
        *out_end = source_file->tokens.count - 2u;
        return 1;
    }

    if (source_file->tokens.count == 0u || node->token_start >= source_file->tokens.count) {
        return 0;
    }

    *out_start = node->token_start;
    *out_end = node->token_end;
    return *out_start <= *out_end;
}

static int ecsvm_ecs_tree_write_token(
    ecsvm_xml_writer_t *writer,
    const ecsvm_source_file_t *source_file,
    size_t token_index
)
{
    const ecsvm_token_t *token;
    size_t line;
    size_t column;

    token = &source_file->tokens.items[token_index];
    line = token->line > 0u ? token->line - 1u : 0u;
    column = token->column > 0u ? token->column - 1u : 0u;

    if (!ecsvm_xml_writer_push_node(writer, "token") ||
        !ecsvm_xml_writer_write_attribute(writer, "kind", ecsvm_ecs_tree_token_kind_string(token->kind)) ||
        !ecsvm_xml_writer_write_attribute_size(writer, "offset", token->offset) ||
        !ecsvm_xml_writer_write_attribute_size(writer, "length", token->length) ||
        !ecsvm_xml_writer_write_attribute_size(writer, "line", line) ||
        !ecsvm_xml_writer_write_attribute_size(writer, "column", column)) {
        return 0;
    }

    if (token->length > 0u &&
        !ecsvm_xml_writer_write_text_range(
            writer,
            source_file->source + token->offset,
            token->length
        )) {
        return 0;
    }

    return ecsvm_xml_writer_pop_node(writer);
}

static int ecsvm_ecs_tree_write_tokens(
    ecsvm_xml_writer_t *writer,
    const ecsvm_source_file_t *source_file,
    size_t start,
    size_t end
)
{
    size_t token_index;

    if (start > end || source_file->tokens.count == 0u) {
        return 1;
    }

    for (token_index = start; token_index <= end; ++token_index) {
        if (!ecsvm_ecs_tree_write_token(writer, source_file, token_index)) {
            return 0;
        }
    }

    return 1;
}

static int ecsvm_ecs_tree_write_syntax_node(
    ecsvm_xml_writer_t *writer,
    const ecsvm_source_file_t *source_file,
    size_t node_index
)
{
    const ecsvm_syntax_node_t *node;
    size_t node_start;
    size_t node_end;
    size_t cursor;
    size_t child_index;

    if (!ecsvm_ecs_tree_node_range(source_file, node_index, &node_start, &node_end)) {
        return 1;
    }

    node = &source_file->nodes.items[node_index];
    if (!ecsvm_xml_writer_push_node(writer, "node") ||
        !ecsvm_xml_writer_write_attribute(writer, "kind", ecsvm_ecs_tree_syntax_kind_string(node->kind))) {
        return 0;
    }

    cursor = node_start;
    for (child_index = node->first_child; child_index != 0u; child_index = source_file->nodes.items[child_index].next_sibling) {
        size_t child_start;
        size_t child_end;

        if (!ecsvm_ecs_tree_node_range(source_file, child_index, &child_start, &child_end)) {
            continue;
        }

        if (child_start > node_end) {
            break;
        }

        if (cursor < child_start &&
            !ecsvm_ecs_tree_write_tokens(writer, source_file, cursor, child_start - 1u)) {
            return 0;
        }

        if (!ecsvm_ecs_tree_write_syntax_node(writer, source_file, child_index)) {
            return 0;
        }

        cursor = child_end + 1u;
    }

    if (cursor <= node_end &&
        !ecsvm_ecs_tree_write_tokens(writer, source_file, cursor, node_end)) {
        return 0;
    }

    return ecsvm_xml_writer_pop_node(writer);
}

static size_t ecsvm_ecs_tree_offset_from_position(
    const ecsvm_source_file_t *source_file,
    size_t line,
    size_t column
)
{
    size_t offset;
    size_t current_line;
    size_t current_column;

    if (source_file == NULL || source_file->source == NULL || line == 0u || column == 0u) {
        return 0u;
    }

    offset = 0u;
    current_line = 1u;
    current_column = 1u;
    while (offset < source_file->length) {
        if (current_line == line && current_column == column) {
            return offset;
        }

        if (source_file->source[offset] == '\n') {
            current_line += 1u;
            current_column = 1u;
        } else {
            current_column += 1u;
        }
        offset += 1u;
    }

    return offset;
}

static void ecsvm_ecs_tree_diagnostic_location(
    const ecsvm_source_file_t *source_file,
    const ecsvm_diagnostic_t *diagnostic,
    size_t *out_offset,
    size_t *out_length
)
{
    size_t token_index;

    *out_offset = 0u;
    *out_length = 0u;
    if (source_file == NULL || diagnostic == NULL) {
        return;
    }

    for (token_index = 0u; token_index < source_file->tokens.count; ++token_index) {
        const ecsvm_token_t *token;

        token = &source_file->tokens.items[token_index];
        if (token->line == diagnostic->line && token->column == diagnostic->column) {
            *out_offset = token->offset;
            *out_length = token->length;
            return;
        }
    }

    *out_offset = ecsvm_ecs_tree_offset_from_position(source_file, diagnostic->line, diagnostic->column);
    if (*out_offset < source_file->length) {
        *out_length = 1u;
    }
}

void ecsvm_ecs_tree_init(
    ecsvm_ecs_tree_t *tree,
    const ecsvm_source_file_t *source_file,
    const ecsvm_diagnostic_t *diagnostic
)
{
    tree->source_file = source_file;
    tree->diagnostic = diagnostic;
}

int ecsvm_ecs_tree_write_xml(
    const ecsvm_ecs_tree_t *tree,
    ecsvm_xml_writer_t *writer
)
{
    const ecsvm_source_file_t *source_file;
    const ecsvm_diagnostic_t *diagnostic;

    source_file = tree != NULL ? tree->source_file : NULL;
    diagnostic = tree != NULL ? tree->diagnostic : NULL;

    if (!ecsvm_xml_writer_push_node(writer, "compilation-unit") ||
        !ecsvm_xml_writer_push_node(writer, "syntax-tree")) {
        return 0;
    }

    if (source_file != NULL && source_file->nodes.count > 1u) {
        if (!ecsvm_ecs_tree_write_syntax_node(writer, source_file, 1u)) {
            return 0;
        }
    } else if (!ecsvm_xml_writer_push_node(writer, "node") ||
               !ecsvm_xml_writer_write_attribute(writer, "kind", "document") ||
               !ecsvm_xml_writer_pop_node(writer)) {
        return 0;
    }

    if (source_file != NULL && source_file->tokens.count > 0u) {
        if (!ecsvm_xml_writer_push_node(writer, "node") ||
            !ecsvm_xml_writer_write_attribute(writer, "kind", "eof") ||
            !ecsvm_ecs_tree_write_token(writer, source_file, source_file->tokens.count - 1u) ||
            !ecsvm_xml_writer_pop_node(writer)) {
            return 0;
        }
    }

    if (!ecsvm_xml_writer_pop_node(writer) ||
        !ecsvm_xml_writer_push_node(writer, "diagnostics")) {
        return 0;
    }

    if (diagnostic != NULL &&
        (diagnostic->code != ECSVM_DIAGNOSTIC_NONE ||
         diagnostic->message[0] != '\0' ||
         diagnostic->file[0] != '\0')) {
        size_t offset;
        size_t length;
        size_t line;
        size_t column;

        ecsvm_ecs_tree_diagnostic_location(source_file, diagnostic, &offset, &length);
        line = diagnostic->line > 0u ? diagnostic->line - 1u : 0u;
        column = diagnostic->column > 0u ? diagnostic->column - 1u : 0u;
        if (!ecsvm_xml_writer_push_node(writer, "diagnostic") ||
            !ecsvm_xml_writer_write_attribute(writer, "severity", "error") ||
            !ecsvm_xml_writer_write_attribute(
                writer,
                "path",
                diagnostic->file[0] != '\0'
                    ? diagnostic->file
                    : (source_file != NULL && source_file->path != NULL ? source_file->path : "")
            ) ||
            !ecsvm_xml_writer_write_attribute_size(writer, "offset", offset) ||
            !ecsvm_xml_writer_write_attribute_size(writer, "length", length) ||
            !ecsvm_xml_writer_write_attribute_size(writer, "line", line) ||
            !ecsvm_xml_writer_write_attribute_size(writer, "column", column) ||
            !ecsvm_xml_writer_write_attribute(
                writer,
                "code",
                diagnostic->code != ECSVM_DIAGNOSTIC_NONE
                    ? ecsvm_diagnostic_code_string(diagnostic->code)
                    : ""
            ) ||
            !ecsvm_xml_writer_write_attribute(writer, "message", diagnostic->message) ||
            !ecsvm_xml_writer_pop_node(writer)) {
            return 0;
        }
    }

    return ecsvm_xml_writer_pop_node(writer) &&
        ecsvm_xml_writer_pop_node(writer);
}
