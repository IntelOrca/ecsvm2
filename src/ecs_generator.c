#include "project_internal.h"

#include <stdlib.h>

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

static uint32_t ecsvm_ast_kind_from_syntax(ecsvm_syntax_kind_t kind)
{
    switch (kind) {
        case ECSVM_SYNTAX_BLOCK: return ECSVM_AST_NODE_BLOCK;
        case ECSVM_SYNTAX_DECLARATION: return ECSVM_AST_NODE_DECLARATION;
        case ECSVM_SYNTAX_RETURN_STATEMENT: return ECSVM_AST_NODE_RETURN_STATEMENT;
        case ECSVM_SYNTAX_IF_STATEMENT: return ECSVM_AST_NODE_IF_STATEMENT;
        case ECSVM_SYNTAX_ELSE_CLAUSE: return ECSVM_AST_NODE_ELSE_CLAUSE;
        case ECSVM_SYNTAX_EXPRESSION_STATEMENT: return ECSVM_AST_NODE_EXPRESSION_STATEMENT;
        case ECSVM_SYNTAX_ASSIGNMENT_EXPRESSION: return ECSVM_AST_NODE_ASSIGNMENT_EXPRESSION;
        case ECSVM_SYNTAX_BINARY_EXPRESSION: return ECSVM_AST_NODE_BINARY_EXPRESSION;
        case ECSVM_SYNTAX_UNARY_EXPRESSION: return ECSVM_AST_NODE_UNARY_EXPRESSION;
        case ECSVM_SYNTAX_CALL_EXPRESSION: return ECSVM_AST_NODE_CALL_EXPRESSION;
        case ECSVM_SYNTAX_ARGUMENT_LIST: return ECSVM_AST_NODE_ARGUMENT_LIST;
        case ECSVM_SYNTAX_MEMBER_EXPRESSION: return ECSVM_AST_NODE_MEMBER_EXPRESSION;
        case ECSVM_SYNTAX_INDEX_EXPRESSION: return ECSVM_AST_NODE_INDEX_EXPRESSION;
        case ECSVM_SYNTAX_GROUPING_EXPRESSION: return ECSVM_AST_NODE_GROUPING_EXPRESSION;
        case ECSVM_SYNTAX_LITERAL_EXPRESSION: return ECSVM_AST_NODE_LITERAL_EXPRESSION;
        case ECSVM_SYNTAX_IDENTIFIER: return ECSVM_AST_NODE_IDENTIFIER;
        case ECSVM_SYNTAX_TYPE_EXPRESSION: return ECSVM_AST_NODE_TYPE_EXPRESSION;
        default:
            return 0u;
    }
}

static int ecsvm_build_function_ast_from_syntax_node(
    const ecsvm_syntax_node_array_t *syntax_nodes,
    size_t syntax_node_index,
    ecsvm_ast_node_array_t *ast_nodes,
    size_t parent_index,
    char *error_message,
    size_t error_message_capacity
)
{
    const ecsvm_syntax_node_t *syntax_node;
    ecsvm_ast_node_t ast_node;
    size_t ast_node_index;
    size_t child_index;

    syntax_node = &syntax_nodes->items[syntax_node_index];
    memset(&ast_node, 0, sizeof(ast_node));
    ast_node.kind = ecsvm_ast_kind_from_syntax(syntax_node->kind);
    /* Source AST nodes temporarily reuse these fields to carry token ranges until serialization. */
    ast_node.token_kind = (uint32_t)syntax_node->token_start;
    ast_node.value_kind = (uint32_t)syntax_node->token_end;
    if (ast_node.kind == 0u) {
        ecsvm_set_error(error_message, error_message_capacity, "unsupported syntax node in function body");
        return 0;
    }

    if (!ecsvm_ast_node_array_push(ast_nodes, ast_node, &ast_node_index)) {
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while building function ast");
        return 0;
    }

    ecsvm_ast_node_add_child(ast_nodes, parent_index, ast_node_index);
    for (child_index = syntax_node->first_child;
         child_index != 0u;
         child_index = syntax_nodes->items[child_index].next_sibling) {
        if (!ecsvm_build_function_ast_from_syntax_node(
                syntax_nodes,
                child_index,
                ast_nodes,
                ast_node_index,
                error_message,
                error_message_capacity
            )) {
            return 0;
        }
    }

    return 1;
}

static int ecsvm_build_function_ast_nodes(
    const ecsvm_syntax_node_array_t *syntax_nodes,
    size_t block_node_index,
    ecsvm_ast_node_array_t *out_nodes,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_ast_node_array_t ast_nodes;

    memset(&ast_nodes, 0, sizeof(ast_nodes));
    if (!ecsvm_ast_node_array_push(
            &ast_nodes,
            (ecsvm_ast_node_t){ ECSVM_AST_NODE_ROOT, 0u, 0u, 0u, 0u, 0u, 0u },
            NULL
        ) ||
        !ecsvm_build_function_ast_from_syntax_node(
            syntax_nodes,
            block_node_index,
            &ast_nodes,
            0u,
            error_message,
            error_message_capacity
        )) {
        free(ast_nodes.items);
        return 0;
    }

    *out_nodes = ast_nodes;
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

static char *ecsvm_attribute_string_value(const char *text)
{
    size_t length;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    if (length >= 2u && text[0] == '"' && text[length - 1u] == '"') {
        return ecsvm_copy_string_range(text + 1, length - 2u);
    }

    return ecsvm_copy_string(text);
}

static char *ecsvm_find_type_alias_target(
    const ecsvm_semantic_struct_array_t *semantic_structs,
    const char *name
)
{
    const char *qualified_alias_prefix;
    const char *alias_name;
    size_t alias_prefix_length;
    size_t index;

    qualified_alias_prefix = NULL;
    alias_name = name;
    alias_prefix_length = 0u;
    if (name != NULL) {
        const char *dot;

        dot = strrchr(name, '.');
        if (dot != NULL) {
            qualified_alias_prefix = name;
            alias_name = dot + 1;
            alias_prefix_length = (size_t)(dot - name);
        }
    }

    for (index = 0u; index < semantic_structs->count; ++index) {
        const ecsvm_semantic_struct_t *semantic_struct;
        size_t attribute_index;

        semantic_struct = &semantic_structs->items[index];
        for (attribute_index = 0u; attribute_index < semantic_struct->attribute_count; ++attribute_index) {
            char *alias_value;
            int matches_prefix;

            if (strcmp(semantic_struct->attributes[attribute_index], "core.Alias") != 0 &&
                strcmp(semantic_struct->attributes[attribute_index], "Alias") != 0) {
                continue;
            }

            alias_value = ecsvm_attribute_string_value(semantic_struct->attribute_data[attribute_index]);
            if (alias_value == NULL) {
                continue;
            }

            matches_prefix = qualified_alias_prefix == NULL ||
                (strncmp(
                     semantic_struct->qualified_name,
                     qualified_alias_prefix,
                     alias_prefix_length
                 ) == 0 &&
                 semantic_struct->qualified_name[alias_prefix_length] == '.');
            if (matches_prefix && strcmp(alias_value, alias_name) == 0) {
                free(alias_value);
                return ecsvm_copy_string(semantic_struct->qualified_name);
            }
            free(alias_value);
        }
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

    qualified_name = ecsvm_find_type_alias_target(semantic_structs, name);
    if (qualified_name != NULL) {
        return qualified_name;
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

static int ecsvm_find_semantic_function_by_suffix(
    const ecsvm_semantic_function_array_t *semantic_functions,
    const char *name
)
{
    size_t index;
    size_t name_length;
    int match_index;

    name_length = strlen(name);
    match_index = -1;
    for (index = 0u; index < semantic_functions->count; ++index) {
        const char *qualified_name;
        size_t qualified_length;

        qualified_name = semantic_functions->items[index].qualified_name;
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

static char *ecsvm_resolve_function_name(
    const ecsvm_semantic_function_array_t *semantic_functions,
    const char *current_namespace,
    const char *name
)
{
    char *qualified_name;
    int index;

    index = ecsvm_find_semantic_function(semantic_functions, name);
    if (index >= 0) {
        return ecsvm_copy_string(semantic_functions->items[index].qualified_name);
    }

    if (strchr(name, '.') != NULL) {
        index = ecsvm_find_semantic_function_by_suffix(semantic_functions, name);
        if (index >= 0) {
            return ecsvm_copy_string(semantic_functions->items[index].qualified_name);
        }
        return ecsvm_copy_string(name);
    }

    qualified_name = ecsvm_join_qualified_name(current_namespace, name);
    if (qualified_name == NULL) {
        return NULL;
    }

    index = ecsvm_find_semantic_function(semantic_functions, qualified_name);
    if (index >= 0) {
        return qualified_name;
    }

    free(qualified_name);
    index = ecsvm_find_semantic_function_by_suffix(semantic_functions, name);
    if (index >= 0) {
        return ecsvm_copy_string(semantic_functions->items[index].qualified_name);
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
        char *namespace_name_part;
        char *child_namespace;
        size_t child_index;

        namespace_name_part = ecsvm_tokens_to_name(file, node->name_start, node->name_end);
        child_namespace = namespace_name_part != NULL
            ? ecsvm_join_qualified_name(namespace_name, namespace_name_part)
            : NULL;
        free(namespace_name_part);
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
                char *attribute_data;

                if (child->name_start == 0u && child->name_end == 0u && child->is_component) {
                    attribute_name = ecsvm_copy_string("core.Component");
                } else if (child->name_start == 0u && child->name_end == 0u && child->is_attribute) {
                    attribute_name = ecsvm_copy_string("core.Attribute");
                } else {
                    attribute_name = ecsvm_tokens_to_name(file, child->name_start, child->name_end);
                }
                attribute_data = (child->value_start != 0u || child->value_end != 0u)
                    ? ecsvm_tokens_to_source(file, child->value_start, child->value_end)
                    : NULL;

                if (attribute_name == NULL ||
                    !ecsvm_semantic_attribute_push(&semantic_struct, attribute_name, attribute_data)) {
                    free(attribute_name);
                    free(attribute_data);
                    ecsvm_semantic_struct_free(&semantic_struct);
                    ecsvm_set_error(error_message, error_message_capacity, "out of memory while collecting attributes");
                    return 0;
                }

                if (strcmp(attribute_name, "core.Component") == 0) {
                    semantic_struct.is_component = 1;
                } else if (strcmp(attribute_name, "core.Attribute") == 0) {
                    semantic_struct.is_attribute = 1;
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

    if (node->kind == ECSVM_SYNTAX_FUNCTION || node->kind == ECSVM_SYNTAX_SYSTEM) {
        ecsvm_semantic_function_t semantic_function;
        size_t child_index;
        size_t body_block_index;

        memset(&semantic_function, 0, sizeof(semantic_function));
        body_block_index = 0u;
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
            if (child->kind == ECSVM_SYNTAX_ATTRIBUTE) {
                char *attribute_name;

                attribute_name = ecsvm_tokens_to_name(file, child->name_start, child->name_end);
                if (attribute_name == NULL ||
                    !ecsvm_semantic_function_attribute_push(&semantic_function, attribute_name)) {
                    free(attribute_name);
                    ecsvm_semantic_function_free(&semantic_function);
                    ecsvm_set_error(error_message, error_message_capacity, "out of memory while collecting function attributes");
                    return 0;
                }
            } else if (child->kind == ECSVM_SYNTAX_PARAMETER_LIST) {
                size_t parameter_index;

                for (parameter_index = child->first_child;
                     parameter_index != 0u;
                     parameter_index = nodes->items[parameter_index].next_sibling) {
                    const ecsvm_syntax_node_t *parameter_node;
                    ecsvm_semantic_parameter_t parameter;

                    parameter_node = &nodes->items[parameter_index];
                    if (parameter_node->kind != ECSVM_SYNTAX_PARAMETER) {
                        continue;
                    }

                    memset(&parameter, 0, sizeof(parameter));
                    parameter.name = ecsvm_tokens_to_name(file, parameter_node->name_start, parameter_node->name_end);
                    parameter.type_name = ecsvm_tokens_to_name(file, parameter_node->type_start, parameter_node->type_end);
                    parameter.default_value = (parameter_node->value_start != 0u || parameter_node->value_end != 0u)
                        ? ecsvm_tokens_to_source(file, parameter_node->value_start, parameter_node->value_end)
                        : NULL;
                    if (parameter.name == NULL || parameter.type_name == NULL ||
                        !ecsvm_semantic_function_parameter_push(&semantic_function, parameter)) {
                        ecsvm_semantic_parameter_free(&parameter);
                        ecsvm_semantic_function_free(&semantic_function);
                        ecsvm_set_error(error_message, error_message_capacity, "out of memory while collecting parameters");
                        return 0;
                    }
                }
            } else if (child->kind == ECSVM_SYNTAX_BLOCK) {
                body_block_index = child_index;
            }
        }

        if (node->kind == ECSVM_SYNTAX_SYSTEM) {
            char *system_attribute_name;

            system_attribute_name = ecsvm_copy_string("core.System");
            if (system_attribute_name == NULL ||
                !ecsvm_semantic_function_attribute_push(&semantic_function, system_attribute_name)) {
                free(system_attribute_name);
                ecsvm_semantic_function_free(&semantic_function);
                ecsvm_set_error(error_message, error_message_capacity, "out of memory while collecting system attribute");
                return 0;
            }
        }

        if (node->has_body &&
            !ecsvm_build_function_ast_nodes(
                nodes,
                body_block_index,
                &semantic_function.body_nodes,
                error_message,
                error_message_capacity
            )) {
            ecsvm_semantic_function_free(&semantic_function);
            return 0;
        }
        semantic_function.body_source_file = node->has_body ? file : NULL;

        if (!ecsvm_semantic_function_array_push(semantic_functions, semantic_function)) {
            ecsvm_semantic_function_free(&semantic_function);
            ecsvm_set_error(error_message, error_message_capacity, "out of memory while collecting functions");
            return 0;
        }
    }

    return 1;
}

int ecsvm_collect_semantics(
    const ecsvm_source_file_array_t *files,
    ecsvm_semantic_struct_array_t *semantic_structs,
    ecsvm_semantic_function_array_t *semantic_functions,
    char *error_message,
    size_t error_message_capacity,
    ecsvm_diagnostic_t *diagnostic
)
{
    size_t file_index;

    (void)diagnostic;

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

int ecsvm_resolve_semantic_types(
    ecsvm_semantic_struct_array_t *semantic_structs,
    ecsvm_semantic_function_array_t *semantic_functions,
    char *error_message,
    size_t error_message_capacity,
    ecsvm_diagnostic_t *diagnostic
)
{
    size_t struct_index;

    (void)diagnostic;

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

    if (semantic_struct->field_count == 0u) {
        size_t builtin_size;
        size_t builtin_alignment;

        builtin_size = ecsvm_builtin_layout(semantic_struct->qualified_name, &builtin_alignment);
        if (builtin_size != 0u) {
            semantic_struct->alignment = builtin_alignment;
            semantic_struct->size = builtin_size;
            semantic_struct->layout_state = 2;
            return 1;
        }
    }

    semantic_struct->alignment = alignment;
    semantic_struct->size = ecsvm_align_up(offset, alignment);
    semantic_struct->layout_state = 2;
    return 1;
}

int ecsvm_compute_layouts(
    ecsvm_semantic_struct_array_t *semantic_structs,
    char *error_message,
    size_t error_message_capacity,
    ecsvm_diagnostic_t *diagnostic
)
{
    size_t index;

    (void)diagnostic;

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

static uint32_t ecsvm_resolve_parameter_id(
    const ecsvm_semantic_function_t *semantic_function,
    const ecsvm_function_ref_builder_t *function_ref,
    const char *text,
    size_t length
)
{
    size_t index;

    if (function_ref->parameter_start == 0u) {
        return 0u;
    }

    for (index = 0u; index < semantic_function->parameter_count; ++index) {
        if (strlen(semantic_function->parameters[index].name) == length &&
            memcmp(semantic_function->parameters[index].name, text, length) == 0) {
            return function_ref->parameter_start + (uint32_t)index;
        }
    }

    return 0u;
}

static int ecsvm_resolve_function_id(
    const ecsvm_semantic_function_array_t *semantic_functions,
    const char *current_namespace,
    const char *text,
    size_t length,
    uint32_t *out_function_id
)
{
    char *name;
    char *qualified_name;
    int function_index;

    *out_function_id = 0u;
    name = ecsvm_copy_string_range(text, length);
    if (name == NULL) {
        return 0;
    }

    qualified_name = ecsvm_resolve_function_name(semantic_functions, current_namespace, name);
    free(name);
    if (qualified_name == NULL) {
        return 0;
    }

    function_index = ecsvm_find_semantic_function(semantic_functions, qualified_name);
    free(qualified_name);
    if (function_index >= 0) {
        *out_function_id = (uint32_t)function_index + 1u;
    }

    return 1;
}

static int ecsvm_resolve_type_id(
    const ecsvm_semantic_struct_array_t *semantic_structs,
    const char *current_namespace,
    const char *text,
    size_t length,
    const ecsvm_type_ref_builder_array_t *type_refs,
    uint32_t *out_type_id
)
{
    char *name;
    char *qualified_name;

    *out_type_id = 0u;
    name = ecsvm_copy_string_range(text, length);
    if (name == NULL) {
        return 0;
    }

    qualified_name = ecsvm_resolve_type_name(semantic_structs, current_namespace, name);
    free(name);
    if (qualified_name == NULL) {
        return 0;
    }

    {
        int type_index;

        type_index = ecsvm_find_type_ref(type_refs, qualified_name);
        *out_type_id = type_index < 0 ? 0u : (uint32_t)type_index + 1u;
    }
    free(qualified_name);
    return 1;
}

static int ecsvm_ast_source_text(
    const ecsvm_source_file_t *file,
    uint32_t token_start,
    uint32_t token_end,
    const char **out_text,
    size_t *out_length
)
{
    size_t start_index;
    size_t end_index;
    size_t start_offset;
    size_t end_offset;

    if (file == NULL || out_text == NULL || out_length == NULL) {
        return 0;
    }

    start_index = (size_t)token_start;
    end_index = (size_t)token_end;
    if (start_index >= file->tokens.count ||
        end_index >= file->tokens.count ||
        start_index > end_index) {
        return 0;
    }

    start_offset = file->tokens.items[start_index].offset;
    end_offset = file->tokens.items[end_index].offset + file->tokens.items[end_index].length;
    if (end_offset < start_offset || end_offset > file->length) {
        return 0;
    }

    *out_text = file->source + start_offset;
    *out_length = end_offset - start_offset;
    return 1;
}

static int ecsvm_serialize_ast_blob_text(
    const ecsvm_source_file_t *file,
    const ecsvm_ast_node_t *source_node,
    ecsvm_blob_array_t *blobs,
    ecsvm_ast_node_t *serialized_node,
    char *error_message,
    size_t error_message_capacity
)
{
    const char *text;
    size_t length;

    if (!ecsvm_ast_source_text(
            file,
            source_node->token_kind,
            source_node->value_kind,
            &text,
            &length
        )) {
        ecsvm_set_error(error_message, error_message_capacity, "invalid source range in function ast");
        return 0;
    }

    serialized_node->value_kind = ECSVM_AST_VALUE_BLOB_ID;
    serialized_node->value = ecsvm_ensure_blob(blobs, text, length);
    if (serialized_node->value == 0u) {
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while serializing function ast");
        return 0;
    }

    return 1;
}

static int ecsvm_serialize_operator_blob(
    const ecsvm_source_file_t *file,
    const ecsvm_ast_node_t *source_node,
    const ecsvm_ast_node_t *left,
    const ecsvm_ast_node_t *right,
    ecsvm_blob_array_t *blobs,
    ecsvm_ast_node_t *serialized_node,
    char *error_message,
    size_t error_message_capacity
)
{
    uint32_t token_start;
    uint32_t token_end;
    const char *text;
    size_t length;

    if (source_node->kind == ECSVM_AST_NODE_UNARY_EXPRESSION) {
        if (left == NULL || source_node->token_kind >= left->token_kind) {
            ecsvm_set_error(error_message, error_message_capacity, "invalid unary operator token range in function ast");
            return 0;
        }
        token_start = source_node->token_kind;
        token_end = left->token_kind - 1u;
    } else {
        if (left == NULL || right == NULL || left->value_kind >= right->token_kind) {
            ecsvm_set_error(error_message, error_message_capacity, "invalid binary operator token ordering in function ast");
            return 0;
        }
        token_start = left->value_kind + 1u;
        token_end = right->token_kind - 1u;
    }

    if (!ecsvm_ast_source_text(file, token_start, token_end, &text, &length)) {
        ecsvm_set_error(error_message, error_message_capacity, "invalid operator range in function ast");
        return 0;
    }

    serialized_node->value_kind = ECSVM_AST_VALUE_BLOB_ID;
    serialized_node->value = ecsvm_ensure_blob(blobs, text, length);
    if (serialized_node->value == 0u) {
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while serializing function ast");
        return 0;
    }

    return 1;
}

static int ecsvm_serialize_function_ast_node(
    const ecsvm_semantic_struct_array_t *semantic_structs,
    const ecsvm_semantic_function_array_t *semantic_functions,
    const ecsvm_semantic_function_t *semantic_function,
    const ecsvm_function_ref_builder_t *function_ref,
    ecsvm_blob_array_t *blobs,
    const ecsvm_type_ref_builder_array_t *type_refs,
    const ecsvm_field_ref_builder_array_t *field_refs,
    uint32_t node_index,
    uint32_t parent_kind,
    uint32_t previous_index,
    ecsvm_ast_node_t *serialized_nodes,
    char *error_message,
    size_t error_message_capacity
)
{
    const ecsvm_ast_node_t *source_node;
    const ecsvm_ast_node_t *first_child;
    const ecsvm_ast_node_t *second_child;

    (void)field_refs;
    source_node = &semantic_function->body_nodes.items[node_index];
    serialized_nodes[node_index] = *source_node;
    serialized_nodes[node_index].token_kind = 0u;
    serialized_nodes[node_index].value_kind = ECSVM_AST_VALUE_NONE;
    serialized_nodes[node_index].value = 0u;

    first_child = source_node->first_child == 0u
        ? NULL
        : &semantic_function->body_nodes.items[source_node->first_child];
    second_child = (first_child != NULL && first_child->next_sibling != 0u)
        ? &semantic_function->body_nodes.items[first_child->next_sibling]
        : NULL;

    if (source_node->kind == ECSVM_AST_NODE_IDENTIFIER) {
        const char *text;
        size_t length;
        uint32_t resolved_id;

        if (!ecsvm_ast_source_text(
                semantic_function->body_source_file,
                source_node->token_kind,
                source_node->value_kind,
                &text,
                &length
            )) {
            ecsvm_set_error(error_message, error_message_capacity, "invalid identifier in function ast");
            return 0;
        }

        resolved_id = ecsvm_resolve_parameter_id(semantic_function, function_ref, text, length);
        if (resolved_id != 0u) {
            serialized_nodes[node_index].value_kind = ECSVM_AST_VALUE_PARAMETER_ID;
            serialized_nodes[node_index].value = resolved_id;
        } else if (parent_kind == ECSVM_AST_NODE_CALL_EXPRESSION &&
                   previous_index == 0u) {
            if (!ecsvm_resolve_function_id(
                    semantic_functions,
                    semantic_function->namespace_name,
                    text,
                    length,
                    &resolved_id
                )) {
                ecsvm_set_error(error_message, error_message_capacity, "out of memory while resolving function references");
                return 0;
            }
            if (resolved_id != 0u) {
                serialized_nodes[node_index].value_kind = ECSVM_AST_VALUE_FUNCTION_REF_ID;
                serialized_nodes[node_index].value = resolved_id;
            } else if (!ecsvm_serialize_ast_blob_text(
                           semantic_function->body_source_file,
                           source_node,
                           blobs,
                           &serialized_nodes[node_index],
                           error_message,
                           error_message_capacity
                       )) {
                return 0;
            }
        } else if (!ecsvm_serialize_ast_blob_text(
                       semantic_function->body_source_file,
                       source_node,
                       blobs,
                       &serialized_nodes[node_index],
                       error_message,
                       error_message_capacity
                   )) {
            return 0;
        }
    } else if (source_node->kind == ECSVM_AST_NODE_TYPE_EXPRESSION) {
        const char *text;
        size_t length;
        uint32_t resolved_id;

        if (!ecsvm_ast_source_text(
                semantic_function->body_source_file,
                source_node->token_kind,
                source_node->value_kind,
                &text,
                &length
            ) ||
            !ecsvm_resolve_type_id(
                semantic_structs,
                semantic_function->namespace_name,
                text,
                length,
                type_refs,
                &resolved_id
            )) {
            ecsvm_set_error(error_message, error_message_capacity, "out of memory while resolving type references");
            return 0;
        }

        if (resolved_id == 0u) {
            ecsvm_set_error(error_message, error_message_capacity, "unresolved type reference in function body");
            return 0;
        }
        serialized_nodes[node_index].value_kind = ECSVM_AST_VALUE_TYPE_REF_ID;
        serialized_nodes[node_index].value = resolved_id;
    } else if (source_node->kind == ECSVM_AST_NODE_LITERAL_EXPRESSION) {
        if (!ecsvm_serialize_ast_blob_text(
                semantic_function->body_source_file,
                source_node,
                blobs,
                &serialized_nodes[node_index],
                error_message,
                error_message_capacity
            )) {
            return 0;
        }
    } else if (source_node->kind == ECSVM_AST_NODE_ASSIGNMENT_EXPRESSION ||
               source_node->kind == ECSVM_AST_NODE_BINARY_EXPRESSION ||
               source_node->kind == ECSVM_AST_NODE_UNARY_EXPRESSION) {
        if (!ecsvm_serialize_operator_blob(
                semantic_function->body_source_file,
                source_node,
                first_child,
                second_child,
                blobs,
                &serialized_nodes[node_index],
                error_message,
                error_message_capacity
            )) {
            return 0;
        }
    }

    {
        uint32_t child_index;
        uint32_t previous_child_index;

        child_index = source_node->first_child;
        previous_child_index = 0u;
        while (child_index != 0u) {
            if (!ecsvm_serialize_function_ast_node(
                    semantic_structs,
                    semantic_functions,
                    semantic_function,
                    function_ref,
                    blobs,
                    type_refs,
                    field_refs,
                    child_index,
                    source_node->kind,
                    previous_child_index,
                    serialized_nodes,
                    error_message,
                    error_message_capacity
                )) {
                return 0;
            }
            previous_child_index = child_index;
            child_index = semantic_function->body_nodes.items[child_index].next_sibling;
        }
    }

    return 1;
}

static int ecsvm_serialize_function_ast_blob(
    const ecsvm_semantic_struct_array_t *semantic_structs,
    const ecsvm_semantic_function_array_t *semantic_functions,
    const ecsvm_semantic_function_t *semantic_function,
    const ecsvm_function_ref_builder_t *function_ref,
    ecsvm_blob_array_t *blobs,
    const ecsvm_type_ref_builder_array_t *type_refs,
    const ecsvm_field_ref_builder_array_t *field_refs,
    unsigned char **out_data,
    size_t *out_length,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_ast_node_t *serialized_nodes;
    unsigned char *data;

    *out_data = NULL;
    *out_length = 0u;
    if (semantic_function->body_nodes.count == 0u) {
        return 1;
    }

    serialized_nodes = (ecsvm_ast_node_t *)malloc(semantic_function->body_nodes.count * sizeof(*serialized_nodes));
    if (serialized_nodes == NULL) {
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while serializing function ast");
        return 0;
    }

    if (!ecsvm_serialize_function_ast_node(
            semantic_structs,
            semantic_functions,
            semantic_function,
            function_ref,
            blobs,
            type_refs,
            field_refs,
            0u,
            ECSVM_AST_NODE_ROOT,
            0u,
            serialized_nodes,
            error_message,
            error_message_capacity
        )) {
        free(serialized_nodes);
        return 0;
    }

    *out_length = sizeof(uint32_t) * 2u + semantic_function->body_nodes.count * sizeof(*serialized_nodes);
    data = (unsigned char *)malloc(*out_length);
    if (data == NULL) {
        free(serialized_nodes);
        ecsvm_set_error(error_message, error_message_capacity, "out of memory while serializing function ast");
        return 0;
    }

    ((uint32_t *)data)[0] = ECSVM_AST_VERSION_3;
    ((uint32_t *)data)[1] = (uint32_t)semantic_function->body_nodes.count;
    memcpy(data + sizeof(uint32_t) * 2u, serialized_nodes, semantic_function->body_nodes.count * sizeof(*serialized_nodes));
    free(serialized_nodes);
    *out_data = data;
    return 1;
}

static const char *ecsvm_c_type_name(const char *qualified_name);

int ecsvm_build_ecsbin_tables(
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
            attribute.data_blob_id = semantic_struct->attribute_data[attribute_index] == NULL
                ? empty_blob_id
                : ecsvm_ensure_blob(
                    blobs,
                    semantic_struct->attribute_data[attribute_index],
                    strlen(semantic_struct->attribute_data[attribute_index])
                );
            if (attribute.type_id == 0u ||
                attribute.data_blob_id == 0u ||
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
        size_t attribute_index;

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
        function_ref.body_blob_id = 0u;
        if (function_ref.namespace_blob_id == 0u ||
            function_ref.name_blob_id == 0u) {
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

        for (attribute_index = 0u; attribute_index < semantic_function->attribute_count; ++attribute_index) {
            ecsvm_attribute_builder_t attribute;

            attribute.type_id = ecsvm_ensure_type_ref(
                type_refs,
                blobs,
                semantic_function->attributes[attribute_index]
            );
            attribute.data_blob_id = empty_blob_id;
            if (attribute.type_id == 0u ||
                !ecsvm_attribute_builder_array_push(attributes, attribute)) {
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

    for (struct_index = 0u; struct_index < semantic_functions->count; ++struct_index) {
        const ecsvm_semantic_function_t *semantic_function;

        semantic_function = &semantic_functions->items[struct_index];
        if (semantic_function->body_nodes.count > 0u) {
            unsigned char *body_ast;
            size_t body_ast_length;
            char ast_error[128];

            if (!ecsvm_serialize_function_ast_blob(
                    semantic_structs,
                    semantic_functions,
                    semantic_function,
                    &function_refs->items[struct_index],
                    blobs,
                    type_refs,
                    field_refs,
                    &body_ast,
                    &body_ast_length,
                    ast_error,
                    sizeof(ast_error)
                )) {
                return 0;
            }
            function_refs->items[struct_index].body_blob_id = ecsvm_ensure_blob(blobs, body_ast, body_ast_length);
            free(body_ast);
            if (function_refs->items[struct_index].body_blob_id == 0u) {
                return 0;
            }
        }
    }

    return 1;
}

int ecsvm_write_ecsbin_file(
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
    if (semantic_struct->field_count == 0u) {
        const char *builtin_type;

        builtin_type = ecsvm_c_type_name(semantic_struct->qualified_name);
        if (builtin_type != NULL) {
            if (fprintf(file, "typedef %s ", builtin_type) < 0 ||
                !ecsvm_write_c_identifier(file, semantic_struct->qualified_name) ||
                fprintf(file, "_t;\n\n") < 0) {
                return 0;
            }
            semantic_struct->emit_state = 2;
            return 1;
        }
    }

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

int ecsvm_write_types_header(
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
