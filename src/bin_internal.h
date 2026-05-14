#ifndef ECSVM_BIN_INTERNAL_H
#define ECSVM_BIN_INTERNAL_H

#include "ecsvm/ecsbin.h"
#include "ecsvm/diagnostic.h"

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

void ecsvm_ecsbin_set_error(char *error_message, size_t error_message_capacity, const char *message);
char *ecsvm_ecsbin_copy_string_range(const unsigned char *data, size_t length);
uint64_t ecsvm_ecsbin_file_size(FILE *file);
int ecsvm_ecsbin_seek(FILE *file, uint64_t offset);
int ecsvm_ecsbin_read_exact(FILE *file, void *data, size_t size);
char *ecsvm_ecsbin_blob_string(const ecsvm_ecsbin_module_t *module, uint32_t blob_id);
char *ecsvm_ecsbin_compose_qualified_name(const char *namespace_name, const char *name);
int ecsvm_ecsbin_range_is_valid(size_t total_count, uint32_t start, uint32_t count);
size_t ecsvm_ecsbin_builtin_layout(const char *qualified_name, size_t *out_alignment);
size_t ecsvm_ecsbin_align_up(size_t value, size_t alignment);
int ecsvm_ecsbin_find_struct_index_by_type(const ecsvm_ecsbin_module_t *module, uint32_t type_id);
ecsvm_status_t ecsvm_ecsbin_compute_struct_layout(ecsvm_ecsbin_module_t *module, size_t struct_index, unsigned char *visit_state, char *error_message, size_t error_message_capacity);

#endif
