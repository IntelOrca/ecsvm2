#ifndef ECSVM_PROJECT_INTERNAL_H
#define ECSVM_PROJECT_INTERNAL_H

#include "ecsvm/project.h"
#include "ecsvm/diagnostic.h"
#include "ecsvm/logger.h"
#include "ecs_syntax_defs.h"
#include "ecsbin_layout.h"
#include "utility.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
#define ECSVM_PROJECT_TOKEN_ENUM(name, assign, text) ECSVM_TOKEN_##name assign,
    ECSVM_TOKEN_KIND_ITEMS(ECSVM_PROJECT_TOKEN_ENUM)
#undef ECSVM_PROJECT_TOKEN_ENUM
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
    ECSVM_SYNTAX_FOR_IN_STATEMENT,
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
     ECSVM_SYNTAX_LITERAL_EXPRESSION,
     ECSVM_SYNTAX_OBJECT_LITERAL,
     ECSVM_SYNTAX_OBJECT_FIELD
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

typedef struct ecsvm_semantic_constant {
    char *namespace_name;
    char *name;
    char *qualified_name;
    char *value_text;
} ecsvm_semantic_constant_t;

typedef enum ecsvm_ast_node_kind {
#define ECSVM_PROJECT_AST_NODE_ENUM(name, assign) ECSVM_AST_NODE_##name assign,
    ECSVM_AST_NODE_KIND_ITEMS(ECSVM_PROJECT_AST_NODE_ENUM)
#undef ECSVM_PROJECT_AST_NODE_ENUM
} ecsvm_ast_node_kind_t;

typedef enum ecsvm_ast_value_kind {
#define ECSVM_PROJECT_AST_VALUE_ENUM(name, assign) ECSVM_AST_VALUE_##name assign,
    ECSVM_AST_VALUE_KIND_ITEMS(ECSVM_PROJECT_AST_VALUE_ENUM)
#undef ECSVM_PROJECT_AST_VALUE_ENUM
} ecsvm_ast_value_kind_t;

typedef ecsvm_shared_ast_node_t ecsvm_ast_node_t;

enum {
    ECSVM_AST_VERSION_3 = 3u
};

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
    char **attribute_data;
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

typedef struct ecsvm_semantic_constant_array {
    ecsvm_semantic_constant_t *items;
    size_t count;
    size_t capacity;
} ecsvm_semantic_constant_array_t;

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

typedef ecsvm_ecsbin_type_ref_disk_t ecsvm_type_ref_disk_t;
typedef ecsvm_ecsbin_field_ref_disk_t ecsvm_field_ref_disk_t;
typedef ecsvm_ecsbin_struct_def_disk_t ecsvm_struct_def_disk_t;
typedef ecsvm_ecsbin_field_def_disk_t ecsvm_field_def_disk_t;
typedef ecsvm_ecsbin_function_ref_disk_t ecsvm_function_ref_disk_t;
typedef ecsvm_ecsbin_parameter_disk_t ecsvm_parameter_disk_t;
typedef ecsvm_ecsbin_attribute_disk_t ecsvm_attribute_disk_t;
typedef ecsvm_ecsbin_blob_disk_t ecsvm_blob_disk_t;


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
int ecsvm_semantic_function_attribute_push(ecsvm_semantic_function_t *function, char *attribute_name, char *attribute_data);
int ecsvm_semantic_function_array_push(ecsvm_semantic_function_array_t *array, ecsvm_semantic_function_t function);
void ecsvm_semantic_parameter_free(ecsvm_semantic_parameter_t *parameter);
void ecsvm_semantic_function_free(ecsvm_semantic_function_t *function);
void ecsvm_semantic_function_array_free(ecsvm_semantic_function_array_t *array);
int ecsvm_semantic_constant_array_push(ecsvm_semantic_constant_array_t *array, ecsvm_semantic_constant_t constant);
void ecsvm_semantic_constant_free(ecsvm_semantic_constant_t *constant);
void ecsvm_semantic_constant_array_free(ecsvm_semantic_constant_array_t *array);
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
int ecsvm_collect_semantics(const ecsvm_source_file_array_t *files, ecsvm_semantic_struct_array_t *semantic_structs, ecsvm_semantic_function_array_t *semantic_functions, ecsvm_semantic_constant_array_t *semantic_constants, char *error_message, size_t error_message_capacity, ecsvm_diagnostic_t *diagnostic);
int ecsvm_resolve_semantic_types(ecsvm_semantic_struct_array_t *semantic_structs, ecsvm_semantic_function_array_t *semantic_functions, char *error_message, size_t error_message_capacity, ecsvm_diagnostic_t *diagnostic);
int ecsvm_compute_layouts(ecsvm_semantic_struct_array_t *semantic_structs, char *error_message, size_t error_message_capacity, ecsvm_diagnostic_t *diagnostic);
int ecsvm_build_ecsbin_tables(const ecsvm_semantic_struct_array_t *semantic_structs, const ecsvm_semantic_function_array_t *semantic_functions, const ecsvm_semantic_constant_array_t *semantic_constants, ecsvm_blob_array_t *blobs, ecsvm_type_ref_builder_array_t *type_refs, ecsvm_field_ref_builder_array_t *field_refs, ecsvm_field_def_builder_array_t *field_defs, ecsvm_function_ref_builder_array_t *function_refs, ecsvm_parameter_builder_array_t *parameters, ecsvm_attribute_builder_array_t *attributes, ecsvm_struct_def_builder_array_t *struct_defs);
int ecsvm_write_ecsbin_file(const char *path, const ecsvm_blob_array_t *blobs, const ecsvm_type_ref_builder_array_t *type_refs, const ecsvm_field_ref_builder_array_t *field_refs, const ecsvm_field_def_builder_array_t *field_defs, const ecsvm_function_ref_builder_array_t *function_refs, const ecsvm_parameter_builder_array_t *parameters, const ecsvm_attribute_builder_array_t *attributes, const ecsvm_struct_def_builder_array_t *struct_defs);
int ecsvm_write_types_header(const char *path, const ecsvm_manifest_t *manifest, ecsvm_semantic_struct_array_t *semantic_structs);

#endif
