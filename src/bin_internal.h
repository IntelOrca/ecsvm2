#ifndef ECSVM_BIN_INTERNAL_H
#define ECSVM_BIN_INTERNAL_H

#include "ecs_syntax_defs.h"
#include "ecsbin_layout.h"
#include "ecsvm/ecsbin.h"
#include "ecsvm/diagnostic.h"
#include "text_buffer.h"
#include "utility.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum ecsvm_ecsbin_token_kind {
#define ECSVM_BIN_TOKEN_ENUM(name, assign, text) ECSVM_ECSBIN_TOKEN_##name assign,
    ECSVM_TOKEN_KIND_ITEMS(ECSVM_BIN_TOKEN_ENUM)
#undef ECSVM_BIN_TOKEN_ENUM
} ecsvm_ecsbin_token_kind_t;

typedef enum ecsvm_ecsbin_ast_node_kind {
#define ECSVM_BIN_AST_NODE_ENUM(name, assign) ECSVM_ECSBIN_AST_NODE_##name assign,
    ECSVM_AST_NODE_KIND_ITEMS(ECSVM_BIN_AST_NODE_ENUM)
#undef ECSVM_BIN_AST_NODE_ENUM
} ecsvm_ecsbin_ast_node_kind_t;

typedef enum ecsvm_ecsbin_ast_value_kind {
#define ECSVM_BIN_AST_VALUE_ENUM(name, assign) ECSVM_ECSBIN_AST_VALUE_##name assign,
    ECSVM_AST_VALUE_KIND_ITEMS(ECSVM_BIN_AST_VALUE_ENUM)
#undef ECSVM_BIN_AST_VALUE_ENUM
} ecsvm_ecsbin_ast_value_kind_t;

typedef ecsvm_shared_ast_node_t ecsvm_ecsbin_ast_node_t;

typedef struct ecsvm_ecsbin_ast_blob {
    const ecsvm_ecsbin_ast_node_t *nodes;
    size_t node_count;
    uint32_t version;
} ecsvm_ecsbin_ast_blob_t;

enum {
    ECSVM_ECSBIN_VERSION_0 = 0u,
    ECSVM_ECSBIN_VERSION_1 = 0u,
    ECSVM_ECSBIN_VERSION_2_V1 = 1u,
    ECSVM_ECSBIN_VERSION_2 = 2u,
    ECSVM_ECSBIN_AST_VERSION_2 = 2u,
    ECSVM_ECSBIN_AST_VERSION_3 = 3u
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
