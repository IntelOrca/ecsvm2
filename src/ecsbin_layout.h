#ifndef ECSVM_ECSBIN_LAYOUT_H
#define ECSVM_ECSBIN_LAYOUT_H

#include <stdint.h>

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

#ifndef ECSVM_ECSBIN_STRUCT_FLAG_COMPONENT
#define ECSVM_ECSBIN_STRUCT_FLAG_COMPONENT 1u
#endif

#endif
