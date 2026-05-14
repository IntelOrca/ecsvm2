#include "project_internal.h"

static const ecsvm_token_t *ecsvm_parser_current(const ecsvm_parser_t *parser)
{
    return &parser->file->tokens.items[parser->index];
}

static ecsvm_token_kind_t ecsvm_parser_peek_kind(const ecsvm_parser_t *parser, size_t lookahead)
{
    size_t index;

    index = parser->index + lookahead;
    if (index >= parser->file->tokens.count) {
        return ECSVM_TOKEN_EOF;
    }

    return parser->file->tokens.items[index].kind;
}

static int ecsvm_parser_match(ecsvm_parser_t *parser, ecsvm_token_kind_t kind)
{
    if (ecsvm_parser_current(parser)->kind != kind) {
        return 0;
    }

    parser->index += 1u;
    return 1;
}

static int ecsvm_parser_match_pair(
    ecsvm_parser_t *parser,
    ecsvm_token_kind_t first,
    ecsvm_token_kind_t second
)
{
    if (ecsvm_parser_peek_kind(parser, 0u) != first ||
        ecsvm_parser_peek_kind(parser, 1u) != second) {
        return 0;
    }

    parser->index += 2u;
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

static int ecsvm_parser_push_node(
    ecsvm_syntax_node_array_t *nodes,
    ecsvm_syntax_kind_t kind,
    size_t token_start,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_syntax_node_t node;

    memset(&node, 0, sizeof(node));
    node.kind = kind;
    node.token_start = token_start;
    if (!ecsvm_syntax_node_array_push(nodes, node, out_node_index)) {
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while building syntax tree");
        return 0;
    }

    return 1;
}

static int ecsvm_parser_push_range_node(
    ecsvm_syntax_node_array_t *nodes,
    ecsvm_syntax_kind_t kind,
    size_t token_start,
    size_t token_end,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    if (!ecsvm_parser_push_node(
            nodes,
            kind,
            token_start,
            out_node_index,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    nodes->items[*out_node_index].token_end = token_end;
    return 1;
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

static int ecsvm_parser_parse_identifier_node(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t *out_start,
    size_t *out_end,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    if (!ecsvm_parser_parse_identifier(
            parser,
            out_start,
            out_end,
            error_message,
            error_message_capacity
        ) ||
        !ecsvm_parser_push_range_node(
            nodes,
            ECSVM_SYNTAX_IDENTIFIER,
            *out_start,
            *out_end,
            out_node_index,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    return 1;
}

static int ecsvm_parser_parse_name_identifier_node(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t *out_start,
    size_t *out_end,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    if (!ecsvm_parser_parse_qualified_name(
            parser,
            out_start,
            out_end,
            error_message,
            error_message_capacity
        ) ||
        !ecsvm_parser_push_range_node(
            nodes,
            ECSVM_SYNTAX_IDENTIFIER,
            *out_start,
            *out_end,
            out_node_index,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    return 1;
}

static int ecsvm_parser_parse_type_expression(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t *out_start,
    size_t *out_end,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    if (!ecsvm_parser_parse_qualified_name(
            parser,
            out_start,
            out_end,
            error_message,
            error_message_capacity
        ) ||
        !ecsvm_parser_push_range_node(
            nodes,
            ECSVM_SYNTAX_TYPE_EXPRESSION,
            *out_start,
            *out_end,
            out_node_index,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

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

static int ecsvm_parser_is_literal_token(ecsvm_token_kind_t kind)
{
    return kind == ECSVM_TOKEN_NUMBER ||
        kind == ECSVM_TOKEN_STRING ||
        kind == ECSVM_TOKEN_KEY_TRUE ||
        kind == ECSVM_TOKEN_KEY_FALSE ||
        kind == ECSVM_TOKEN_KEY_NULL;
}

static int ecsvm_parser_is_unary_operator(ecsvm_token_kind_t kind)
{
    return kind == ECSVM_TOKEN_BANG ||
        kind == ECSVM_TOKEN_PLUS ||
        kind == ECSVM_TOKEN_MINUS ||
        kind == ECSVM_TOKEN_TILDE;
}

static int ecsvm_parser_find_generic_call_open(
    const ecsvm_parser_t *parser,
    size_t *out_open_paren
)
{
    size_t index;
    size_t depth;

    if (ecsvm_parser_peek_kind(parser, 0u) != ECSVM_TOKEN_LT) {
        return 0;
    }

    index = parser->index;
    depth = 0u;
    while (index < parser->file->tokens.count) {
        ecsvm_token_kind_t kind;

        kind = parser->file->tokens.items[index].kind;
        if (kind == ECSVM_TOKEN_LT) {
            depth += 1u;
        } else if (kind == ECSVM_TOKEN_GT) {
            if (depth == 0u) {
                return 0;
            }
            depth -= 1u;
            if (depth == 0u) {
                index += 1u;
                if (index < parser->file->tokens.count &&
                    parser->file->tokens.items[index].kind == ECSVM_TOKEN_LPAREN) {
                    *out_open_paren = index;
                    return 1;
                }
                return 0;
            }
        } else if (kind == ECSVM_TOKEN_EOF ||
                   kind == ECSVM_TOKEN_SEMICOLON ||
                   kind == ECSVM_TOKEN_LBRACE ||
                   kind == ECSVM_TOKEN_RBRACE) {
            return 0;
        }
        index += 1u;
    }

    return 0;
}

static int ecsvm_parser_parse_expression(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
);

static int ecsvm_parser_parse_argument_list(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t argument_list_index;

    if (!ecsvm_parser_push_node(
            nodes,
            ECSVM_SYNTAX_ARGUMENT_LIST,
            parser->index,
            &argument_list_index,
            error_message,
            error_message_capacity
        ) ||
        !ecsvm_parser_expect(parser, ECSVM_TOKEN_LPAREN, error_message, error_message_capacity)) {
        return 0;
    }

    while (ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_RPAREN &&
           ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_EOF) {
        size_t argument_index;

        if (!ecsvm_parser_parse_expression(
                parser,
                nodes,
                &argument_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }

        ecsvm_syntax_node_add_child(nodes, argument_list_index, argument_index);
        if (!ecsvm_parser_match(parser, ECSVM_TOKEN_COMMA)) {
            break;
        }
    }

    if (!ecsvm_parser_expect(parser, ECSVM_TOKEN_RPAREN, error_message, error_message_capacity)) {
        return 0;
    }

    nodes->items[argument_list_index].token_end = parser->index - 1u;
    *out_node_index = argument_list_index;
    return 1;
}

static int ecsvm_parser_parse_primary_expression(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    if (ecsvm_parser_current(parser)->kind == ECSVM_TOKEN_IDENTIFIER) {
        size_t start;
        size_t end;

        return ecsvm_parser_parse_identifier_node(
            parser,
            nodes,
            &start,
            &end,
            out_node_index,
            error_message,
            error_message_capacity
        );
    }

    if (ecsvm_parser_is_literal_token(ecsvm_parser_current(parser)->kind)) {
        if (!ecsvm_parser_push_range_node(
                nodes,
                ECSVM_SYNTAX_LITERAL_EXPRESSION,
                parser->index,
                parser->index,
                out_node_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }
        parser->index += 1u;
        return 1;
    }

    if (ecsvm_parser_current(parser)->kind == ECSVM_TOKEN_LPAREN) {
        size_t grouping_index;
        size_t expression_index;

        if (!ecsvm_parser_push_node(
                nodes,
                ECSVM_SYNTAX_GROUPING_EXPRESSION,
                parser->index,
                &grouping_index,
                error_message,
                error_message_capacity
            ) ||
            !ecsvm_parser_expect(parser, ECSVM_TOKEN_LPAREN, error_message, error_message_capacity) ||
            !ecsvm_parser_parse_expression(
                parser,
                nodes,
                &expression_index,
                error_message,
                error_message_capacity
            ) ||
            !ecsvm_parser_expect(parser, ECSVM_TOKEN_RPAREN, error_message, error_message_capacity)) {
            return 0;
        }

        ecsvm_syntax_node_add_child(nodes, grouping_index, expression_index);
        nodes->items[grouping_index].token_end = parser->index - 1u;
        *out_node_index = grouping_index;
        return 1;
    }

    ecsvm_set_error(error_message, error_message_capacity, "expected expression");
    return 0;
}

static int ecsvm_parser_parse_postfix_expression(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t expression_index;

    if (!ecsvm_parser_parse_primary_expression(
            parser,
            nodes,
            &expression_index,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    while (1) {
        if (ecsvm_parser_current(parser)->kind == ECSVM_TOKEN_DOT) {
            size_t member_index;
            size_t identifier_index;
            size_t name_start;
            size_t name_end;

            parser->index += 1u;
            if (!ecsvm_parser_parse_identifier_node(
                    parser,
                    nodes,
                    &name_start,
                    &name_end,
                    &identifier_index,
                    error_message,
                    error_message_capacity
                ) ||
                !ecsvm_parser_push_node(
                    nodes,
                    ECSVM_SYNTAX_MEMBER_EXPRESSION,
                    nodes->items[expression_index].token_start,
                    &member_index,
                    error_message,
                    error_message_capacity
                )) {
                return 0;
            }

            ecsvm_syntax_node_add_child(nodes, member_index, expression_index);
            ecsvm_syntax_node_add_child(nodes, member_index, identifier_index);
            nodes->items[member_index].token_end = name_end;
            expression_index = member_index;
            continue;
        }

        if (ecsvm_parser_current(parser)->kind == ECSVM_TOKEN_LBRACKET) {
            size_t index_expression_index;
            size_t inner_expression_index;

            if (!ecsvm_parser_expect(parser, ECSVM_TOKEN_LBRACKET, error_message, error_message_capacity) ||
                !ecsvm_parser_parse_expression(
                    parser,
                    nodes,
                    &inner_expression_index,
                    error_message,
                    error_message_capacity
                ) ||
                !ecsvm_parser_expect(parser, ECSVM_TOKEN_RBRACKET, error_message, error_message_capacity) ||
                !ecsvm_parser_push_node(
                    nodes,
                    ECSVM_SYNTAX_INDEX_EXPRESSION,
                    nodes->items[expression_index].token_start,
                    &index_expression_index,
                    error_message,
                    error_message_capacity
                )) {
                return 0;
            }

            ecsvm_syntax_node_add_child(nodes, index_expression_index, expression_index);
            ecsvm_syntax_node_add_child(nodes, index_expression_index, inner_expression_index);
            nodes->items[index_expression_index].token_end = parser->index - 1u;
            expression_index = index_expression_index;
            continue;
        }

        if (ecsvm_parser_current(parser)->kind == ECSVM_TOKEN_LPAREN ||
            ecsvm_parser_current(parser)->kind == ECSVM_TOKEN_LT) {
            size_t open_paren_index;
            size_t argument_list_index;
            size_t call_index;
            size_t saved_index;

            saved_index = parser->index;
            if (ecsvm_parser_current(parser)->kind == ECSVM_TOKEN_LT) {
                if (!ecsvm_parser_find_generic_call_open(parser, &open_paren_index)) {
                    break;
                }
                parser->index = open_paren_index;
            }

            if (!ecsvm_parser_parse_argument_list(
                    parser,
                    nodes,
                    &argument_list_index,
                    error_message,
                    error_message_capacity
                ) ||
                !ecsvm_parser_push_node(
                    nodes,
                    ECSVM_SYNTAX_CALL_EXPRESSION,
                    nodes->items[expression_index].token_start,
                    &call_index,
                    error_message,
                    error_message_capacity
                )) {
                return 0;
            }

            ecsvm_syntax_node_add_child(nodes, call_index, expression_index);
            ecsvm_syntax_node_add_child(nodes, call_index, argument_list_index);
            nodes->items[call_index].token_end = nodes->items[argument_list_index].token_end;
            expression_index = call_index;
            if (saved_index != parser->index) {
                continue;
            }
            continue;
        }

        break;
    }

    *out_node_index = expression_index;
    return 1;
}

static int ecsvm_parser_parse_unary_expression(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    if (ecsvm_parser_is_unary_operator(ecsvm_parser_current(parser)->kind)) {
        size_t unary_index;
        size_t operand_index;
        size_t token_start;

        token_start = parser->index;
        parser->index += 1u;
        if (!ecsvm_parser_parse_unary_expression(
                parser,
                nodes,
                &operand_index,
                error_message,
                error_message_capacity
            ) ||
            !ecsvm_parser_push_node(
                nodes,
                ECSVM_SYNTAX_UNARY_EXPRESSION,
                token_start,
                &unary_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }

        ecsvm_syntax_node_add_child(nodes, unary_index, operand_index);
        nodes->items[unary_index].token_end = nodes->items[operand_index].token_end;
        *out_node_index = unary_index;
        return 1;
    }

    return ecsvm_parser_parse_postfix_expression(
        parser,
        nodes,
        out_node_index,
        error_message,
        error_message_capacity
    );
}

static int ecsvm_parser_parse_multiplicative_expression(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t expression_index;

    if (!ecsvm_parser_parse_unary_expression(
            parser,
            nodes,
            &expression_index,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    while (ecsvm_parser_current(parser)->kind == ECSVM_TOKEN_STAR ||
           ecsvm_parser_current(parser)->kind == ECSVM_TOKEN_SLASH ||
           ecsvm_parser_current(parser)->kind == ECSVM_TOKEN_PERCENT) {
        size_t binary_index;
        size_t right_index;

        parser->index += 1u;
        if (!ecsvm_parser_parse_unary_expression(
                parser,
                nodes,
                &right_index,
                error_message,
                error_message_capacity
            ) ||
            !ecsvm_parser_push_node(
                nodes,
                ECSVM_SYNTAX_BINARY_EXPRESSION,
                nodes->items[expression_index].token_start,
                &binary_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }

        ecsvm_syntax_node_add_child(nodes, binary_index, expression_index);
        ecsvm_syntax_node_add_child(nodes, binary_index, right_index);
        nodes->items[binary_index].token_end = nodes->items[right_index].token_end;
        expression_index = binary_index;
    }

    *out_node_index = expression_index;
    return 1;
}

static int ecsvm_parser_parse_additive_expression(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t expression_index;

    if (!ecsvm_parser_parse_multiplicative_expression(
            parser,
            nodes,
            &expression_index,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    while (ecsvm_parser_current(parser)->kind == ECSVM_TOKEN_PLUS ||
           ecsvm_parser_current(parser)->kind == ECSVM_TOKEN_MINUS) {
        size_t binary_index;
        size_t right_index;

        parser->index += 1u;
        if (!ecsvm_parser_parse_multiplicative_expression(
                parser,
                nodes,
                &right_index,
                error_message,
                error_message_capacity
            ) ||
            !ecsvm_parser_push_node(
                nodes,
                ECSVM_SYNTAX_BINARY_EXPRESSION,
                nodes->items[expression_index].token_start,
                &binary_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }

        ecsvm_syntax_node_add_child(nodes, binary_index, expression_index);
        ecsvm_syntax_node_add_child(nodes, binary_index, right_index);
        nodes->items[binary_index].token_end = nodes->items[right_index].token_end;
        expression_index = binary_index;
    }

    *out_node_index = expression_index;
    return 1;
}

static int ecsvm_parser_parse_comparison_expression(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t expression_index;

    if (!ecsvm_parser_parse_additive_expression(
            parser,
            nodes,
            &expression_index,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    while (ecsvm_parser_current(parser)->kind == ECSVM_TOKEN_LT ||
           ecsvm_parser_current(parser)->kind == ECSVM_TOKEN_GT) {
        size_t binary_index;
        size_t right_index;

        parser->index += 1u;
        if (ecsvm_parser_current(parser)->kind == ECSVM_TOKEN_EQUAL) {
            parser->index += 1u;
        }
        if (!ecsvm_parser_parse_additive_expression(
                parser,
                nodes,
                &right_index,
                error_message,
                error_message_capacity
            ) ||
            !ecsvm_parser_push_node(
                nodes,
                ECSVM_SYNTAX_BINARY_EXPRESSION,
                nodes->items[expression_index].token_start,
                &binary_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }

        ecsvm_syntax_node_add_child(nodes, binary_index, expression_index);
        ecsvm_syntax_node_add_child(nodes, binary_index, right_index);
        nodes->items[binary_index].token_end = nodes->items[right_index].token_end;
        expression_index = binary_index;
    }

    *out_node_index = expression_index;
    return 1;
}

static int ecsvm_parser_parse_equality_expression(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t expression_index;

    if (!ecsvm_parser_parse_comparison_expression(
            parser,
            nodes,
            &expression_index,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    while (ecsvm_parser_match_pair(parser, ECSVM_TOKEN_EQUAL, ECSVM_TOKEN_EQUAL) ||
           ecsvm_parser_match_pair(parser, ECSVM_TOKEN_BANG, ECSVM_TOKEN_EQUAL)) {
        size_t binary_index;
        size_t right_index;

        if (!ecsvm_parser_parse_comparison_expression(
                parser,
                nodes,
                &right_index,
                error_message,
                error_message_capacity
            ) ||
            !ecsvm_parser_push_node(
                nodes,
                ECSVM_SYNTAX_BINARY_EXPRESSION,
                nodes->items[expression_index].token_start,
                &binary_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }

        ecsvm_syntax_node_add_child(nodes, binary_index, expression_index);
        ecsvm_syntax_node_add_child(nodes, binary_index, right_index);
        nodes->items[binary_index].token_end = nodes->items[right_index].token_end;
        expression_index = binary_index;
    }

    *out_node_index = expression_index;
    return 1;
}

static int ecsvm_parser_parse_logical_and_expression(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t expression_index;

    if (!ecsvm_parser_parse_equality_expression(
            parser,
            nodes,
            &expression_index,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    while (ecsvm_parser_match_pair(parser, ECSVM_TOKEN_AMPERSAND, ECSVM_TOKEN_AMPERSAND)) {
        size_t binary_index;
        size_t right_index;

        if (!ecsvm_parser_parse_equality_expression(
                parser,
                nodes,
                &right_index,
                error_message,
                error_message_capacity
            ) ||
            !ecsvm_parser_push_node(
                nodes,
                ECSVM_SYNTAX_BINARY_EXPRESSION,
                nodes->items[expression_index].token_start,
                &binary_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }

        ecsvm_syntax_node_add_child(nodes, binary_index, expression_index);
        ecsvm_syntax_node_add_child(nodes, binary_index, right_index);
        nodes->items[binary_index].token_end = nodes->items[right_index].token_end;
        expression_index = binary_index;
    }

    *out_node_index = expression_index;
    return 1;
}

static int ecsvm_parser_parse_logical_or_expression(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t expression_index;

    if (!ecsvm_parser_parse_logical_and_expression(
            parser,
            nodes,
            &expression_index,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    while (ecsvm_parser_match_pair(parser, ECSVM_TOKEN_PIPE, ECSVM_TOKEN_PIPE)) {
        size_t binary_index;
        size_t right_index;

        if (!ecsvm_parser_parse_logical_and_expression(
                parser,
                nodes,
                &right_index,
                error_message,
                error_message_capacity
            ) ||
            !ecsvm_parser_push_node(
                nodes,
                ECSVM_SYNTAX_BINARY_EXPRESSION,
                nodes->items[expression_index].token_start,
                &binary_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }

        ecsvm_syntax_node_add_child(nodes, binary_index, expression_index);
        ecsvm_syntax_node_add_child(nodes, binary_index, right_index);
        nodes->items[binary_index].token_end = nodes->items[right_index].token_end;
        expression_index = binary_index;
    }

    *out_node_index = expression_index;
    return 1;
}

static int ecsvm_parser_parse_assignment_expression(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t left_index;

    if (!ecsvm_parser_parse_logical_or_expression(
            parser,
            nodes,
            &left_index,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    if (ecsvm_parser_current(parser)->kind == ECSVM_TOKEN_EQUAL &&
        ecsvm_parser_peek_kind(parser, 1u) != ECSVM_TOKEN_EQUAL) {
        size_t assignment_index;
        size_t right_index;

        parser->index += 1u;
        if (!ecsvm_parser_parse_assignment_expression(
                parser,
                nodes,
                &right_index,
                error_message,
                error_message_capacity
            ) ||
            !ecsvm_parser_push_node(
                nodes,
                ECSVM_SYNTAX_ASSIGNMENT_EXPRESSION,
                nodes->items[left_index].token_start,
                &assignment_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }

        ecsvm_syntax_node_add_child(nodes, assignment_index, left_index);
        ecsvm_syntax_node_add_child(nodes, assignment_index, right_index);
        nodes->items[assignment_index].token_end = nodes->items[right_index].token_end;
        *out_node_index = assignment_index;
        return 1;
    }

    *out_node_index = left_index;
    return 1;
}

static int ecsvm_parser_parse_expression(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    return ecsvm_parser_parse_assignment_expression(
        parser,
        nodes,
        out_node_index,
        error_message,
        error_message_capacity
    );
}

static int ecsvm_parser_parse_block(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
);

static int ecsvm_parser_parse_statement(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
);

static int ecsvm_parser_parse_declaration_statement(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t token_start,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t declaration_index;
    size_t identifier_index;
    size_t type_index;
    size_t value_index;

    type_index = 0u;
    value_index = 0u;
    if (!ecsvm_parser_push_node(
            nodes,
            ECSVM_SYNTAX_DECLARATION,
            token_start,
            &declaration_index,
            error_message,
            error_message_capacity
        ) ||
        !ecsvm_parser_parse_identifier_node(
            parser,
            nodes,
            &nodes->items[declaration_index].name_start,
            &nodes->items[declaration_index].name_end,
            &identifier_index,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    ecsvm_syntax_node_add_child(nodes, declaration_index, identifier_index);
    if (ecsvm_parser_match(parser, ECSVM_TOKEN_COLON)) {
        if (!ecsvm_parser_parse_type_expression(
                parser,
                nodes,
                &nodes->items[declaration_index].type_start,
                &nodes->items[declaration_index].type_end,
                &type_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }
        ecsvm_syntax_node_add_child(nodes, declaration_index, type_index);
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_EQUAL)) {
        if (!ecsvm_parser_parse_expression(
                parser,
                nodes,
                &value_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }
        nodes->items[declaration_index].value_start = nodes->items[value_index].token_start;
        nodes->items[declaration_index].value_end = nodes->items[value_index].token_end;
        ecsvm_syntax_node_add_child(nodes, declaration_index, value_index);
    }

    if (!ecsvm_parser_expect(parser, ECSVM_TOKEN_SEMICOLON, error_message, error_message_capacity)) {
        return 0;
    }

    nodes->items[declaration_index].token_end = parser->index - 1u;
    *out_node_index = declaration_index;
    return 1;
}

static int ecsvm_parser_parse_return_statement(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t token_start,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t return_index;

    if (!ecsvm_parser_push_node(
            nodes,
            ECSVM_SYNTAX_RETURN_STATEMENT,
            token_start,
            &return_index,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    if (ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_SEMICOLON) {
        size_t expression_index;

        if (!ecsvm_parser_parse_expression(
                parser,
                nodes,
                &expression_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }
        nodes->items[return_index].value_start = nodes->items[expression_index].token_start;
        nodes->items[return_index].value_end = nodes->items[expression_index].token_end;
        ecsvm_syntax_node_add_child(nodes, return_index, expression_index);
    }

    if (!ecsvm_parser_expect(parser, ECSVM_TOKEN_SEMICOLON, error_message, error_message_capacity)) {
        return 0;
    }

    nodes->items[return_index].token_end = parser->index - 1u;
    *out_node_index = return_index;
    return 1;
}

static int ecsvm_parser_parse_if_statement(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t token_start,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t if_index;
    size_t condition_index;
    size_t then_index;

    if (!ecsvm_parser_push_node(
            nodes,
            ECSVM_SYNTAX_IF_STATEMENT,
            token_start,
            &if_index,
            error_message,
            error_message_capacity
        ) ||
        !ecsvm_parser_expect(parser, ECSVM_TOKEN_LPAREN, error_message, error_message_capacity) ||
        !ecsvm_parser_parse_expression(
            parser,
            nodes,
            &condition_index,
            error_message,
            error_message_capacity
        ) ||
        !ecsvm_parser_expect(parser, ECSVM_TOKEN_RPAREN, error_message, error_message_capacity) ||
        !ecsvm_parser_parse_statement(
            parser,
            nodes,
            &then_index,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    ecsvm_syntax_node_add_child(nodes, if_index, condition_index);
    ecsvm_syntax_node_add_child(nodes, if_index, then_index);
    nodes->items[if_index].token_end = nodes->items[then_index].token_end;

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_KEY_ELSE)) {
        size_t else_index;
        size_t else_child_index;

        if (!ecsvm_parser_push_node(
                nodes,
                ECSVM_SYNTAX_ELSE_CLAUSE,
                parser->index - 1u,
                &else_index,
                error_message,
                error_message_capacity
            ) ||
            !ecsvm_parser_parse_statement(
                parser,
                nodes,
                &else_child_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }

        ecsvm_syntax_node_add_child(nodes, else_index, else_child_index);
        nodes->items[else_index].token_end = nodes->items[else_child_index].token_end;
        ecsvm_syntax_node_add_child(nodes, if_index, else_index);
        nodes->items[if_index].token_end = nodes->items[else_index].token_end;
    }

    *out_node_index = if_index;
    return 1;
}

static int ecsvm_parser_parse_expression_statement(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t statement_index;
    size_t expression_index;

    if (!ecsvm_parser_push_node(
            nodes,
            ECSVM_SYNTAX_EXPRESSION_STATEMENT,
            parser->index,
            &statement_index,
            error_message,
            error_message_capacity
        ) ||
        !ecsvm_parser_parse_expression(
            parser,
            nodes,
            &expression_index,
            error_message,
            error_message_capacity
        ) ||
        !ecsvm_parser_expect(parser, ECSVM_TOKEN_SEMICOLON, error_message, error_message_capacity)) {
        return 0;
    }

    ecsvm_syntax_node_add_child(nodes, statement_index, expression_index);
    nodes->items[statement_index].value_start = nodes->items[expression_index].token_start;
    nodes->items[statement_index].value_end = nodes->items[expression_index].token_end;
    nodes->items[statement_index].token_end = parser->index - 1u;
    *out_node_index = statement_index;
    return 1;
}

static int ecsvm_parser_parse_statement(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    if (ecsvm_parser_current(parser)->kind == ECSVM_TOKEN_LBRACE) {
        return ecsvm_parser_parse_block(
            parser,
            nodes,
            out_node_index,
            error_message,
            error_message_capacity
        );
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_KEY_IF)) {
        return ecsvm_parser_parse_if_statement(
            parser,
            nodes,
            parser->index - 1u,
            out_node_index,
            error_message,
            error_message_capacity
        );
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_KEY_RETURN)) {
        return ecsvm_parser_parse_return_statement(
            parser,
            nodes,
            parser->index - 1u,
            out_node_index,
            error_message,
            error_message_capacity
        );
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_KEY_LET)) {
        return ecsvm_parser_parse_declaration_statement(
            parser,
            nodes,
            parser->index - 1u,
            out_node_index,
            error_message,
            error_message_capacity
        );
    }

    return ecsvm_parser_parse_expression_statement(
        parser,
        nodes,
        out_node_index,
        error_message,
        error_message_capacity
    );
}

static int ecsvm_parser_parse_block(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t *out_node_index,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t block_index;

    if (!ecsvm_parser_push_node(
            nodes,
            ECSVM_SYNTAX_BLOCK,
            parser->index,
            &block_index,
            error_message,
            error_message_capacity
        ) ||
        !ecsvm_parser_expect(parser, ECSVM_TOKEN_LBRACE, error_message, error_message_capacity)) {
        return 0;
    }

    while (ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_RBRACE &&
           ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_EOF) {
        size_t statement_index;

        if (!ecsvm_parser_parse_statement(
                parser,
                nodes,
                &statement_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }
        ecsvm_syntax_node_add_child(nodes, block_index, statement_index);
    }

    if (!ecsvm_parser_expect(parser, ECSVM_TOKEN_RBRACE, error_message, error_message_capacity)) {
        return 0;
    }

    nodes->items[block_index].token_end = parser->index - 1u;
    *out_node_index = block_index;
    return 1;
}

static int ecsvm_parser_parse_import(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t parent_index,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_syntax_node_t import_node;
    size_t import_index;
    size_t start;
    size_t end;

    memset(&import_node, 0, sizeof(import_node));
    import_node.kind = ECSVM_SYNTAX_IMPORT;
    import_node.token_start = parser->index - 1u;
    if (!ecsvm_parser_parse_qualified_name(
            parser,
            &start,
            &end,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    import_node.name_start = start;
    import_node.name_end = end;
    if (!ecsvm_parser_expect(parser, ECSVM_TOKEN_SEMICOLON, error_message, error_message_capacity)) {
        return 0;
    }

    import_node.token_end = parser->index - 1u;
    if (!ecsvm_syntax_node_array_push(nodes, import_node, &import_index)) {
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while building syntax tree");
        return 0;
    }

    ecsvm_syntax_node_add_child(nodes, parent_index, import_index);
    return 1;
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
        size_t identifier_index;
        size_t type_index;

        memset(&field, 0, sizeof(field));
        field.kind = ECSVM_SYNTAX_FIELD;
        field.token_start = parser->index;
        if (!ecsvm_parser_parse_name_identifier_node(
                parser,
                nodes,
                &field.name_start,
                &field.name_end,
                &identifier_index,
                error_message,
                error_message_capacity
            ) ||
            !ecsvm_parser_expect(parser, ECSVM_TOKEN_COLON, error_message, error_message_capacity) ||
            !ecsvm_parser_parse_type_expression(
                parser,
                nodes,
                &field.type_start,
                &field.type_end,
                &type_index,
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

        ecsvm_syntax_node_add_child(nodes, field_index, identifier_index);
        ecsvm_syntax_node_add_child(nodes, field_index, type_index);
        ecsvm_syntax_node_add_child(nodes, struct_index, field_index);
    }

    return ecsvm_parser_expect(parser, ECSVM_TOKEN_RBRACE, error_message, error_message_capacity);
}

static int ecsvm_parser_parse_parameter(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t parameter_list_index,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_syntax_node_t parameter;
    size_t parameter_index;
    size_t identifier_index;
    size_t type_index;

    memset(&parameter, 0, sizeof(parameter));
    parameter.kind = ECSVM_SYNTAX_PARAMETER;
    parameter.token_start = parser->index;
    if (!ecsvm_parser_parse_identifier_node(
            parser,
            nodes,
            &parameter.name_start,
            &parameter.name_end,
            &identifier_index,
            error_message,
            error_message_capacity
        ) ||
        !ecsvm_parser_expect(parser, ECSVM_TOKEN_COLON, error_message, error_message_capacity) ||
        !ecsvm_parser_parse_type_expression(
            parser,
            nodes,
            &parameter.type_start,
            &parameter.type_end,
            &type_index,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_EQUAL)) {
        size_t value_index;

        if (!ecsvm_parser_parse_expression(
                parser,
                nodes,
                &value_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }
        parameter.value_start = nodes->items[value_index].token_start;
        parameter.value_end = nodes->items[value_index].token_end;
        parameter.token_end = parameter.value_end;
        if (!ecsvm_syntax_node_array_push(nodes, parameter, &parameter_index)) {
            ecsvm_set_error(error_message, error_message_capacity, "out of memory while building syntax tree");
            return 0;
        }
        ecsvm_syntax_node_add_child(nodes, parameter_index, identifier_index);
        ecsvm_syntax_node_add_child(nodes, parameter_index, type_index);
        ecsvm_syntax_node_add_child(nodes, parameter_index, value_index);
        ecsvm_syntax_node_add_child(nodes, parameter_list_index, parameter_index);
        return 1;
    }

    parameter.token_end = parameter.type_end;
    if (!ecsvm_syntax_node_array_push(nodes, parameter, &parameter_index)) {
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while building syntax tree");
        return 0;
    }

    ecsvm_syntax_node_add_child(nodes, parameter_index, identifier_index);
    ecsvm_syntax_node_add_child(nodes, parameter_index, type_index);
    ecsvm_syntax_node_add_child(nodes, parameter_list_index, parameter_index);
    return 1;
}

static int ecsvm_parser_parse_parameter_list(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t parent_index,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t parameter_list_index;

    if (!ecsvm_parser_push_node(
            nodes,
            ECSVM_SYNTAX_PARAMETER_LIST,
            parser->index,
            &parameter_list_index,
            error_message,
            error_message_capacity
        ) ||
        !ecsvm_parser_expect(parser, ECSVM_TOKEN_LPAREN, error_message, error_message_capacity)) {
        return 0;
    }

    while (ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_RPAREN &&
           ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_EOF) {
        if (!ecsvm_parser_parse_parameter(
                parser,
                nodes,
                parameter_list_index,
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

    nodes->items[parameter_list_index].token_end = parser->index - 1u;
    ecsvm_syntax_node_add_child(nodes, parent_index, parameter_list_index);
    return 1;
}

static int ecsvm_parser_parse_function_like(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t parent_index,
    ecsvm_syntax_kind_t kind,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_syntax_node_t function_node;
    size_t function_index;

    memset(&function_node, 0, sizeof(function_node));
    function_node.kind = kind;
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

    if (kind == ECSVM_SYNTAX_FUNCTION || ecsvm_parser_current(parser)->kind == ECSVM_TOKEN_LPAREN) {
        if (!ecsvm_parser_parse_parameter_list(
                parser,
                nodes,
                function_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }
    }

    if (kind == ECSVM_SYNTAX_FUNCTION && ecsvm_parser_match(parser, ECSVM_TOKEN_COLON)) {
        size_t type_index;

        if (!ecsvm_parser_parse_type_expression(
                parser,
                nodes,
                &nodes->items[function_index].type_start,
                &nodes->items[function_index].type_end,
                &type_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }
        ecsvm_syntax_node_add_child(nodes, function_index, type_index);
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_SEMICOLON)) {
        nodes->items[function_index].token_end = parser->index - 1u;
        ecsvm_syntax_node_add_child(nodes, parent_index, function_index);
        return 1;
    }

    if (ecsvm_parser_current(parser)->kind != ECSVM_TOKEN_LBRACE) {
        ecsvm_set_error(error_message, error_message_capacity, "expected block");
        return 0;
    }

    {
        size_t block_index;

        if (!ecsvm_parser_parse_block(
                parser,
                nodes,
                &block_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }

        nodes->items[function_index].body_start = nodes->items[block_index].token_start;
        nodes->items[function_index].body_end = nodes->items[block_index].token_end;
        nodes->items[function_index].has_body = 1;
        nodes->items[function_index].token_end = nodes->items[block_index].token_end;
        ecsvm_syntax_node_add_child(nodes, function_index, block_index);
    }

    ecsvm_syntax_node_add_child(nodes, parent_index, function_index);
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

    if (!ecsvm_parser_expect(parser, ECSVM_TOKEN_RBRACE, error_message, error_message_capacity)) {
        return 0;
    }

    nodes->items[namespace_index].token_end = parser->index - 1u;
    ecsvm_syntax_node_add_child(nodes, parent_index, namespace_index);
    return 1;
}

static int ecsvm_parser_add_implicit_attribute(
    ecsvm_syntax_node_array_t *nodes,
    size_t struct_index,
    int is_component,
    int is_attribute,
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
    attribute.is_component = is_component;
    attribute.is_attribute = is_attribute;
    if (!ecsvm_syntax_node_array_push(nodes, attribute, &attribute_index)) {
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while building syntax tree");
        return 0;
    }

    nodes->items[struct_index].is_component = is_component;
    nodes->items[struct_index].is_attribute = is_attribute;
    ecsvm_syntax_node_add_child(nodes, struct_index, attribute_index);
    return 1;
}

static int ecsvm_parser_parse_struct_like(
    ecsvm_parser_t *parser,
    ecsvm_syntax_node_array_t *nodes,
    size_t parent_index,
    int implicit_component,
    int implicit_attribute,
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
    struct_node.is_attribute = implicit_attribute;
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

    if ((implicit_component || implicit_attribute) &&
        !ecsvm_parser_add_implicit_attribute(
            nodes,
            struct_index,
            implicit_component,
            implicit_attribute,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_SEMICOLON)) {
        nodes->items[struct_index].token_end = parser->index - 1u;
        ecsvm_syntax_node_add_child(nodes, parent_index, struct_index);
        return 1;
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
    ecsvm_syntax_node_add_child(nodes, parent_index, struct_index);
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
        )) {
        return 0;
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_LPAREN)) {
        size_t value_index;
        size_t value_start;

        value_start = parser->index - 1u;
        if (!ecsvm_parser_parse_expression(
                parser,
                nodes,
                &value_index,
                error_message,
                error_message_capacity
            ) ||
            !ecsvm_parser_expect(parser, ECSVM_TOKEN_RPAREN, error_message, error_message_capacity)) {
            return 0;
        }
        attribute.value_start = nodes->items[value_index].token_start;
        attribute.value_end = nodes->items[value_index].token_end;
        (void)value_start;
    }

    if (!ecsvm_parser_expect(parser, ECSVM_TOKEN_RBRACKET, error_message, error_message_capacity)) {
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
        return ecsvm_parser_parse_import(
            parser,
            nodes,
            parent_index,
            error_message,
            error_message_capacity
        );
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

        if (ecsvm_parser_match(parser, ECSVM_TOKEN_KEY_COMPONENT)) {
            nodes->items[struct_index].is_component = 1;
            if (!ecsvm_parser_add_implicit_attribute(
                    nodes,
                    struct_index,
                    1,
                    0,
                    error_message,
                    error_message_capacity
                )) {
                return 0;
            }
        } else if (ecsvm_parser_match(parser, ECSVM_TOKEN_KEY_ATTRIBUTE)) {
            nodes->items[struct_index].is_attribute = 1;
            if (!ecsvm_parser_add_implicit_attribute(
                    nodes,
                    struct_index,
                    0,
                    1,
                    error_message,
                    error_message_capacity
                )) {
                return 0;
            }
        } else if (!ecsvm_parser_expect(parser, ECSVM_TOKEN_KEY_STRUCT, error_message, error_message_capacity)) {
            return 0;
        }

        if (!ecsvm_parser_parse_qualified_name(
                parser,
                &nodes->items[struct_index].name_start,
                &nodes->items[struct_index].name_end,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }

        if (ecsvm_parser_match(parser, ECSVM_TOKEN_SEMICOLON)) {
            nodes->items[struct_index].token_end = parser->index - 1u;
            ecsvm_syntax_node_add_child(nodes, parent_index, struct_index);
            return 1;
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
        ecsvm_syntax_node_add_child(nodes, parent_index, struct_index);
        return 1;
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_KEY_STRUCT)) {
        return ecsvm_parser_parse_struct_like(
            parser,
            nodes,
            parent_index,
            0,
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
            0,
            error_message,
            error_message_capacity
        );
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_KEY_ATTRIBUTE)) {
        return ecsvm_parser_parse_struct_like(
            parser,
            nodes,
            parent_index,
            0,
            1,
            error_message,
            error_message_capacity
        );
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_KEY_FN)) {
        return ecsvm_parser_parse_function_like(
            parser,
            nodes,
            parent_index,
            ECSVM_SYNTAX_FUNCTION,
            error_message,
            error_message_capacity
        );
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_KEY_SYSTEM)) {
        return ecsvm_parser_parse_function_like(
            parser,
            nodes,
            parent_index,
            ECSVM_SYNTAX_SYSTEM,
            error_message,
            error_message_capacity
        );
    }

    if (ecsvm_parser_match(parser, ECSVM_TOKEN_KEY_CONST)) {
        size_t declaration_index;

        if (!ecsvm_parser_parse_declaration_statement(
                parser,
                nodes,
                parser->index - 1u,
                &declaration_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }
        ecsvm_syntax_node_add_child(nodes, parent_index, declaration_index);
        return 1;
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
                const ecsvm_token_t *token;

                token = ecsvm_parser_current(&parser);
                ecsvm_diagnostic_set(
                    diagnostic,
                    file->path,
                    token->line,
                    token->column,
                    ECSVM_DIAGNOSTIC_UNEXPECTED_TOKEN,
                    error_message
                );
            }
            return 0;
        }
    }

    return 1;
}
