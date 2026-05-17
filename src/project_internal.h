#ifndef ECSVM_PROJECT_INTERNAL_H
#define ECSVM_PROJECT_INTERNAL_H

#include "ecsvm/project.h"
#include "ecsvm/diagnostic.h"
#include "ecsvm/logger.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
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
    ECSVM_TOKEN_KEY_ATTRIBUTE,
    ECSVM_TOKEN_KEY_SYSTEM,
    ECSVM_TOKEN_KEY_CONST,
    ECSVM_TOKEN_KEY_FN,
    ECSVM_TOKEN_KEY_IF,
    ECSVM_TOKEN_KEY_ELSE,
    ECSVM_TOKEN_KEY_LET,
    ECSVM_TOKEN_KEY_RETURN,
    ECSVM_TOKEN_KEY_TRUE,
    ECSVM_TOKEN_KEY_FALSE,
    ECSVM_TOKEN_KEY_NULL
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
    ECSVM_SYNTAX_SYSTEM,
    ECSVM_SYNTAX_PARAMETER_LIST,
    ECSVM_SYNTAX_PARAMETER,
    ECSVM_SYNTAX_TYPE_EXPRESSION,
    ECSVM_SYNTAX_IDENTIFIER,
    ECSVM_SYNTAX_BLOCK,
    ECSVM_SYNTAX_DECLARATION,
    ECSVM_SYNTAX_RETURN_STATEMENT,
    ECSVM_SYNTAX_IF_STATEMENT,
    ECSVM_SYNTAX_ELSE_CLAUSE,
    ECSVM_SYNTAX_EXPRESSION_STATEMENT,
    ECSVM_SYNTAX_ASSIGNMENT_EXPRESSION,
    ECSVM_SYNTAX_BINARY_EXPRESSION,
    ECSVM_SYNTAX_UNARY_EXPRESSION,
    ECSVM_SYNTAX_CALL_EXPRESSION,
    ECSVM_SYNTAX_ARGUMENT_LIST,
    ECSVM_SYNTAX_MEMBER_EXPRESSION,
    ECSVM_SYNTAX_INDEX_EXPRESSION,
    ECSVM_SYNTAX_GROUPING_EXPRESSION,
    ECSVM_SYNTAX_LITERAL_EXPRESSION
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
    int is_attribute;
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
    char **attribute_data;
    size_t attribute_count;
    size_t attribute_capacity;
    size_t size;
    size_t alignment;
    int is_component;
    int is_attribute;
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

typedef enum ecsvm_ast_node_kind {
    ECSVM_AST_NODE_ROOT = 1,
    ECSVM_AST_NODE_BLOCK,
    ECSVM_AST_NODE_GROUP_PAREN = 3,
    ECSVM_AST_NODE_GROUP_BRACKET = 4,
    ECSVM_AST_NODE_TOKEN = 5,
    ECSVM_AST_NODE_DECLARATION,
    ECSVM_AST_NODE_RETURN_STATEMENT,
    ECSVM_AST_NODE_IF_STATEMENT,
    ECSVM_AST_NODE_ELSE_CLAUSE,
    ECSVM_AST_NODE_EXPRESSION_STATEMENT,
    ECSVM_AST_NODE_ASSIGNMENT_EXPRESSION,
    ECSVM_AST_NODE_BINARY_EXPRESSION,
    ECSVM_AST_NODE_UNARY_EXPRESSION,
    ECSVM_AST_NODE_CALL_EXPRESSION,
    ECSVM_AST_NODE_ARGUMENT_LIST,
    ECSVM_AST_NODE_MEMBER_EXPRESSION,
    ECSVM_AST_NODE_INDEX_EXPRESSION,
    ECSVM_AST_NODE_GROUPING_EXPRESSION,
    ECSVM_AST_NODE_LITERAL_EXPRESSION,
    ECSVM_AST_NODE_IDENTIFIER,
    ECSVM_AST_NODE_TYPE_EXPRESSION
} ecsvm_ast_node_kind_t;

typedef enum ecsvm_ast_value_kind {
    ECSVM_AST_VALUE_NONE = 0,
    ECSVM_AST_VALUE_BLOB_ID,
    ECSVM_AST_VALUE_TYPE_REF_ID,
    ECSVM_AST_VALUE_FIELD_REF_ID,
    ECSVM_AST_VALUE_FUNCTION_REF_ID,
    ECSVM_AST_VALUE_PARAMETER_ID
} ecsvm_ast_value_kind_t;

typedef struct ecsvm_ast_node {
    uint32_t kind;
    uint32_t first_child;
    uint32_t last_child;
    uint32_t next_sibling;
    uint32_t token_kind;
    uint32_t value_kind;
    uint32_t value;
} ecsvm_ast_node_t;

typedef struct ecsvm_ast_node_array {
    ecsvm_ast_node_t *items;
    size_t count;
    size_t capacity;
} ecsvm_ast_node_array_t;

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
    const ecsvm_source_file_t *body_source_file;
    ecsvm_ast_node_array_t body_nodes;
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

#ifndef ECSVM_ECSBIN_STRUCT_FLAG_COMPONENT
#define ECSVM_ECSBIN_STRUCT_FLAG_COMPONENT 1u
#endif


void ecsvm_set_error(char *error_message, size_t capacity, const char *message);
char *ecsvm_copy_string(const char *text);
char *ecsvm_copy_string_range(const char *text, size_t length);
size_t ecsvm_next_capacity(size_t current, size_t minimum);
int ecsvm_reserve_bytes(void **items, size_t item_size, size_t *capacity, size_t minimum);
int ecsvm_string_array_push(ecsvm_string_array_t *array, char *value);
void ecsvm_string_array_free(ecsvm_string_array_t *array);
int ecsvm_token_array_push(ecsvm_token_array_t *array, ecsvm_token_t token);
int ecsvm_syntax_node_array_push(ecsvm_syntax_node_array_t *array, ecsvm_syntax_node_t node, size_t *out_index);
void ecsvm_syntax_node_add_child(ecsvm_syntax_node_array_t *array, size_t parent_index, size_t child_index);
int ecsvm_source_file_array_push(ecsvm_source_file_array_t *array, ecsvm_source_file_t file);
void ecsvm_source_file_free(ecsvm_source_file_t *file);
void ecsvm_source_file_array_free(ecsvm_source_file_array_t *array);
int ecsvm_semantic_field_array_push(ecsvm_semantic_struct_t *semantic_struct, ecsvm_semantic_field_t field);
int ecsvm_semantic_attribute_push(ecsvm_semantic_struct_t *semantic_struct, char *attribute, char *data);
int ecsvm_semantic_struct_array_push(ecsvm_semantic_struct_array_t *array, ecsvm_semantic_struct_t semantic_struct);
void ecsvm_semantic_struct_free(ecsvm_semantic_struct_t *semantic_struct);
void ecsvm_semantic_struct_array_free(ecsvm_semantic_struct_array_t *array);
int ecsvm_semantic_function_parameter_push(ecsvm_semantic_function_t *function, ecsvm_semantic_parameter_t parameter);
int ecsvm_semantic_function_attribute_push(ecsvm_semantic_function_t *function, char *attribute_name);
int ecsvm_semantic_function_array_push(ecsvm_semantic_function_array_t *array, ecsvm_semantic_function_t function);
void ecsvm_semantic_parameter_free(ecsvm_semantic_parameter_t *parameter);
void ecsvm_semantic_function_free(ecsvm_semantic_function_t *function);
void ecsvm_semantic_function_array_free(ecsvm_semantic_function_array_t *array);
int ecsvm_blob_array_push(ecsvm_blob_array_t *array, ecsvm_blob_entry_t entry);
void ecsvm_blob_array_free(ecsvm_blob_array_t *array);
int ecsvm_type_ref_builder_array_push(ecsvm_type_ref_builder_array_t *array, ecsvm_type_ref_builder_t type_ref);
void ecsvm_type_ref_builder_array_free(ecsvm_type_ref_builder_array_t *array);
int ecsvm_field_ref_builder_array_push(ecsvm_field_ref_builder_array_t *array, ecsvm_field_ref_builder_t field_ref);
void ecsvm_field_ref_builder_array_free(ecsvm_field_ref_builder_array_t *array);
int ecsvm_field_def_builder_array_push(ecsvm_field_def_builder_array_t *array, ecsvm_field_def_builder_t field_def);
void ecsvm_field_def_builder_array_free(ecsvm_field_def_builder_array_t *array);
int ecsvm_function_ref_builder_array_push(ecsvm_function_ref_builder_array_t *array, ecsvm_function_ref_builder_t function_ref);
void ecsvm_function_ref_builder_array_free(ecsvm_function_ref_builder_array_t *array);
int ecsvm_parameter_builder_array_push(ecsvm_parameter_builder_array_t *array, ecsvm_parameter_builder_t parameter);
void ecsvm_parameter_builder_array_free(ecsvm_parameter_builder_array_t *array);
int ecsvm_attribute_builder_array_push(ecsvm_attribute_builder_array_t *array, ecsvm_attribute_builder_t attribute);
void ecsvm_attribute_builder_array_free(ecsvm_attribute_builder_array_t *array);
int ecsvm_struct_def_builder_array_push(ecsvm_struct_def_builder_array_t *array, ecsvm_struct_def_builder_t struct_def);
void ecsvm_struct_def_builder_array_free(ecsvm_struct_def_builder_array_t *array);
int ecsvm_ast_node_array_push(ecsvm_ast_node_array_t *array, ecsvm_ast_node_t node, size_t *out_index);
void ecsvm_ast_node_add_child(ecsvm_ast_node_array_t *array, size_t parent_index, size_t child_index);

int ecsvm_lex_source(ecsvm_source_file_t *file, char *error_message, size_t error_message_capacity, ecsvm_diagnostic_t *diagnostic);
int ecsvm_parser_find_matching_token(const ecsvm_parser_t *parser, size_t start_index, ecsvm_token_kind_t open_kind, ecsvm_token_kind_t close_kind, size_t *out_end_index);
int ecsvm_parse_file(ecsvm_source_file_t *file, char *error_message, size_t error_message_capacity, ecsvm_diagnostic_t *diagnostic);
int ecsvm_collect_semantics(const ecsvm_source_file_array_t *files, ecsvm_semantic_struct_array_t *semantic_structs, ecsvm_semantic_function_array_t *semantic_functions, char *error_message, size_t error_message_capacity, ecsvm_diagnostic_t *diagnostic);
int ecsvm_resolve_semantic_types(ecsvm_semantic_struct_array_t *semantic_structs, ecsvm_semantic_function_array_t *semantic_functions, char *error_message, size_t error_message_capacity, ecsvm_diagnostic_t *diagnostic);
int ecsvm_compute_layouts(ecsvm_semantic_struct_array_t *semantic_structs, char *error_message, size_t error_message_capacity, ecsvm_diagnostic_t *diagnostic);
int ecsvm_build_ecsbin_tables(const ecsvm_semantic_struct_array_t *semantic_structs, const ecsvm_semantic_function_array_t *semantic_functions, ecsvm_blob_array_t *blobs, ecsvm_type_ref_builder_array_t *type_refs, ecsvm_field_ref_builder_array_t *field_refs, ecsvm_field_def_builder_array_t *field_defs, ecsvm_function_ref_builder_array_t *function_refs, ecsvm_parameter_builder_array_t *parameters, ecsvm_attribute_builder_array_t *attributes, ecsvm_struct_def_builder_array_t *struct_defs);
int ecsvm_write_ecsbin_file(const char *path, const ecsvm_blob_array_t *blobs, const ecsvm_type_ref_builder_array_t *type_refs, const ecsvm_field_ref_builder_array_t *field_refs, const ecsvm_field_def_builder_array_t *field_defs, const ecsvm_function_ref_builder_array_t *function_refs, const ecsvm_parameter_builder_array_t *parameters, const ecsvm_attribute_builder_array_t *attributes, const ecsvm_struct_def_builder_array_t *struct_defs);
int ecsvm_write_types_header(const char *path, const ecsvm_manifest_t *manifest, ecsvm_semantic_struct_array_t *semantic_structs);

#endif
