#include "project_internal.h"

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

int ecsvm_parser_find_matching_token(
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

int ecsvm_parse_file(
    ecsvm_source_file_t *file,
    char *error_message,
    size_t error_message_capacity,
    ecsvm_diagnostic_t *diagnostic
)
{
    ecsvm_parser_t parser;
    ecsvm_syntax_node_t root;
    ecsvm_syntax_node_t file_node;
    size_t root_index;
    size_t file_index;

    memset(&parser, 0, sizeof(parser));
    parser.file = file;
    if (diagnostic != NULL) {
        ecsvm_diagnostic_clear(diagnostic);
    }
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
            if (diagnostic != NULL && diagnostic->code == ECSVM_DIAGNOSTIC_NONE) {
                const ecsvm_token_t *token = ecsvm_parser_current(&parser);
                ecsvm_diagnostic_set(diagnostic, file->path, token->line, token->column, ECSVM_DIAGNOSTIC_UNEXPECTED_TOKEN, error_message);
            }
            return 0;
        }
    }

    return 1;
}
