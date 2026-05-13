#include "ecsvm/ecsbin.h"

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

typedef struct ecsvm_ecsbin_header {
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

typedef struct ecsvm_ecsbin_attribute_disk {
    uint32_t type_id;
    uint32_t data_blob_id;
} ecsvm_ecsbin_attribute_disk_t;

typedef struct ecsvm_ecsbin_blob_disk {
    uint64_t offset;
    uint64_t length;
} ecsvm_ecsbin_blob_disk_t;

enum {
    ECSVM_ECSBIN_VERSION_0 = 0u,
    ECSVM_ECSBIN_VERSION_1 = 0u,
    ECSVM_ECSBIN_VERSION_2 = 1u
};

_Static_assert(sizeof(ecsvm_ecsbin_header_t) == 80u, "ecsbin header size");
_Static_assert(sizeof(ecsvm_ecsbin_blob_disk_t) == 16u, "ecsbin blob size");

static void ecsvm_ecsbin_set_error(
    char *error_message,
    size_t error_message_capacity,
    const char *message
)
{
    if (error_message == NULL || error_message_capacity == 0u) {
        return;
    }

    if (message == NULL) {
        error_message[0] = '\0';
        return;
    }

    (void)snprintf(error_message, error_message_capacity, "%s", message);
}

static char *ecsvm_ecsbin_copy_string_range(const unsigned char *data, size_t length)
{
    char *copy;

    copy = (char *)malloc(length + 1u);
    if (copy == NULL) {
        return NULL;
    }

    if (length > 0u) {
        memcpy(copy, data, length);
    }
    copy[length] = '\0';
    return copy;
}

static uint64_t ecsvm_ecsbin_file_size(FILE *file)
{
    ecsvm_file_offset_t current;
    ecsvm_file_offset_t end;

    current = ECSVM_FTELL(file);
    if (current < 0) {
        return 0u;
    }

    if (ECSVM_FSEEK(file, 0, SEEK_END) != 0) {
        return 0u;
    }

    end = ECSVM_FTELL(file);
    if (end < 0) {
        return 0u;
    }

    if (ECSVM_FSEEK(file, current, SEEK_SET) != 0) {
        return 0u;
    }

    return (uint64_t)end;
}

static int ecsvm_ecsbin_seek(FILE *file, uint64_t offset)
{
    return ECSVM_FSEEK(file, (ecsvm_file_offset_t)offset, SEEK_SET) == 0;
}

static int ecsvm_ecsbin_read_exact(FILE *file, void *data, size_t size)
{
    return size == 0u || fread(data, 1u, size, file) == size;
}

static char *ecsvm_ecsbin_blob_string(
    const ecsvm_ecsbin_module_t *module,
    uint32_t blob_id
)
{
    const ecsvm_ecsbin_blob_t *blob;

    if (module == NULL || blob_id == 0u || blob_id > module->blob_count) {
        return NULL;
    }

    blob = &module->blobs[blob_id - 1u];
    return ecsvm_ecsbin_copy_string_range(blob->data, (size_t)blob->length);
}

static size_t ecsvm_ecsbin_builtin_layout(
    const char *qualified_name,
    size_t *out_alignment
)
{
    size_t size;
    size_t alignment;

    size = 0u;
    alignment = 0u;
    if (qualified_name == NULL) {
        if (out_alignment != NULL) {
            *out_alignment = 0u;
        }
        return 0u;
    }

    if (strcmp(qualified_name, "core.Entity") == 0) {
        size = sizeof(ecsvm_entity_t);
        alignment = ECSVM_ALIGNOF(ecsvm_entity_t);
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
        size = sizeof(ecsvm_blob_t);
        alignment = ECSVM_ALIGNOF(ecsvm_blob_t);
    } else if (strcmp(qualified_name, "core.Bool") == 0) {
        size = sizeof(unsigned char);
        alignment = ECSVM_ALIGNOF(unsigned char);
    }

    if (out_alignment != NULL) {
        *out_alignment = alignment;
    }
    return size;
}

static size_t ecsvm_ecsbin_align_up(size_t value, size_t alignment)
{
    if (alignment == 0u) {
        return value;
    }

    return (value + alignment - 1u) / alignment * alignment;
}

static int ecsvm_ecsbin_find_struct_index_by_type(
    const ecsvm_ecsbin_module_t *module,
    uint32_t type_id
)
{
    size_t index;

    if (module == NULL || type_id == 0u) {
        return -1;
    }

    for (index = 0u; index < module->struct_def_count; ++index) {
        if (module->struct_defs[index].type_id == type_id) {
            return (int)index;
        }
    }

    return -1;
}

static ecsvm_status_t ecsvm_ecsbin_compute_struct_layout(
    ecsvm_ecsbin_module_t *module,
    size_t struct_index,
    unsigned char *visit_state,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_ecsbin_struct_def_t *definition;
    size_t size;
    size_t alignment;
    size_t field_index;

    if (module == NULL || struct_index >= module->struct_def_count || visit_state == NULL) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "invalid struct layout request");
        return ECSVM_ERROR_ARGUMENT;
    }

    if (visit_state[struct_index] == 2u) {
        return ECSVM_OK;
    }

    if (visit_state[struct_index] == 1u) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "recursive struct definitions are not supported");
        return ECSVM_ERROR_ARGUMENT;
    }

    visit_state[struct_index] = 1u;
    definition = &module->struct_defs[struct_index];
    size = 0u;
    alignment = 1u;

    for (field_index = 0u; field_index < definition->field_count; ++field_index) {
        const ecsvm_ecsbin_field_ref_t *field_ref;
        const ecsvm_ecsbin_type_ref_t *type_ref;
        size_t field_size;
        size_t field_alignment;
        int nested_index;

        if (definition->field_start == 0u ||
            definition->field_start - 1u + field_index >= module->field_ref_count) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "field range is out of bounds");
            return ECSVM_ERROR_ARGUMENT;
        }

        field_ref = &module->field_refs[definition->field_start - 1u + field_index];
        type_ref = ecsvm_ecsbin_type_ref(module, field_ref->type_id);
        if (type_ref == NULL || type_ref->qualified_name == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "field type reference is invalid");
            return ECSVM_ERROR_ARGUMENT;
        }

        field_size = ecsvm_ecsbin_builtin_layout(type_ref->qualified_name, &field_alignment);
        if (field_size == 0u) {
            nested_index = ecsvm_ecsbin_find_struct_index_by_type(module, field_ref->type_id);
            if (nested_index < 0) {
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "field type does not resolve to a known struct");
                return ECSVM_ERROR_ARGUMENT;
            }

            if (ecsvm_ecsbin_compute_struct_layout(
                    module,
                    (size_t)nested_index,
                    visit_state,
                    error_message,
                    error_message_capacity
                ) != ECSVM_OK) {
                return ECSVM_ERROR_ARGUMENT;
            }

            field_size = module->struct_defs[nested_index].size;
            field_alignment = module->struct_defs[nested_index].alignment;
        }

        size = ecsvm_ecsbin_align_up(size, field_alignment);
        size += field_size;
        if (field_alignment > alignment) {
            alignment = field_alignment;
        }
    }

    definition->alignment = alignment;
    definition->size = ecsvm_ecsbin_align_up(size, alignment);
    visit_state[struct_index] = 2u;
    return ECSVM_OK;
}

const ecsvm_ecsbin_type_ref_t *ecsvm_ecsbin_type_ref(
    const ecsvm_ecsbin_module_t *module,
    uint32_t type_id
)
{
    if (module == NULL || type_id == 0u || type_id > module->type_ref_count) {
        return NULL;
    }

    return &module->type_refs[type_id - 1u];
}

const ecsvm_ecsbin_struct_def_t *ecsvm_ecsbin_find_struct(
    const ecsvm_ecsbin_module_t *module,
    const char *qualified_name
)
{
    size_t index;

    if (module == NULL || qualified_name == NULL) {
        return NULL;
    }

    for (index = 0u; index < module->struct_def_count; ++index) {
        const ecsvm_ecsbin_type_ref_t *type_ref;

        type_ref = ecsvm_ecsbin_type_ref(module, module->struct_defs[index].type_id);
        if (type_ref != NULL &&
            type_ref->qualified_name != NULL &&
            strcmp(type_ref->qualified_name, qualified_name) == 0) {
            return &module->struct_defs[index];
        }
    }

    return NULL;
}

int ecsvm_ecsbin_struct_is_component(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_struct_def_t *definition
)
{
    size_t attribute_index;

    if (module == NULL || definition == NULL) {
        return 0;
    }

    if ((definition->flags & ECSVM_ECSBIN_STRUCT_FLAG_COMPONENT) != 0u) {
        return 1;
    }

    for (attribute_index = 0u; attribute_index < definition->attribute_count; ++attribute_index) {
        const ecsvm_ecsbin_attribute_t *attribute;
        const ecsvm_ecsbin_type_ref_t *type_ref;

        if (definition->attribute_start == 0u ||
            definition->attribute_start - 1u + attribute_index >= module->attribute_count) {
            return 0;
        }

        attribute = &module->attributes[definition->attribute_start - 1u + attribute_index];
        type_ref = ecsvm_ecsbin_type_ref(module, attribute->type_id);
        if (type_ref != NULL &&
            type_ref->qualified_name != NULL &&
            strcmp(type_ref->qualified_name, "core.Component") == 0) {
            return 1;
        }
    }

    return 0;
}

ecsvm_status_t ecsvm_ecsbin_register_components(
    ecsvm_engine_t *engine,
    const ecsvm_ecsbin_module_t *module
)
{
    size_t index;

    if (engine == NULL || module == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    for (index = 0u; index < module->struct_def_count; ++index) {
        const ecsvm_ecsbin_struct_def_t *definition;
        const ecsvm_ecsbin_type_ref_t *type_ref;
        ecsvm_component_desc_t desc;
        ecsvm_status_t status;

        definition = &module->struct_defs[index];
        if (!ecsvm_ecsbin_struct_is_component(module, definition)) {
            continue;
        }

        type_ref = ecsvm_ecsbin_type_ref(module, definition->type_id);
        if (type_ref == NULL || type_ref->qualified_name == NULL || definition->size == 0u) {
            return ECSVM_ERROR_ARGUMENT;
        }

        desc.name = type_ref->qualified_name;
        desc.size = definition->size;
        desc.preferred_storage = ECSVM_STORAGE_CONTIGUOUS;
        status = ecsvm_engine_register_component(engine, &desc, NULL);
        if (status != ECSVM_OK) {
            return status;
        }
    }

    return ECSVM_OK;
}

void ecsvm_ecsbin_unload(ecsvm_ecsbin_module_t *module)
{
    size_t index;

    if (module == NULL) {
        return;
    }

    for (index = 0u; index < module->type_ref_count; ++index) {
        free(module->type_refs[index].namespace_name);
        free(module->type_refs[index].name);
        free(module->type_refs[index].qualified_name);
    }

    for (index = 0u; index < module->field_ref_count; ++index) {
        free(module->field_refs[index].name);
    }

    for (index = 0u; index < module->attribute_count; ++index) {
        free(module->attributes[index].data);
    }

    for (index = 0u; index < module->blob_count; ++index) {
        free(module->blobs[index].data);
    }

    free(module->type_refs);
    free(module->field_refs);
    free(module->struct_defs);
    free(module->field_defs);
    free(module->attributes);
    free(module->blobs);
    memset(module, 0, sizeof(*module));
}

ecsvm_status_t ecsvm_ecsbin_load(
    const char *path,
    ecsvm_ecsbin_module_t *out_module,
    char *error_message,
    size_t error_message_capacity
)
{
    FILE *file;
    ecsvm_ecsbin_module_t module;
    ecsvm_ecsbin_header_t header;
    uint64_t file_size;
    uint64_t blob_data_offset;
    size_t index;
    unsigned char *visit_state;
    ecsvm_status_t status;

    ecsvm_ecsbin_set_error(error_message, error_message_capacity, NULL);
    if (path == NULL || out_module == NULL) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "path and output module are required");
        return ECSVM_ERROR_ARGUMENT;
    }

    memset(&module, 0, sizeof(module));
    file = fopen(path, "rb");
    if (file == NULL) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to open ecsbin file");
        return ECSVM_ERROR_NOT_FOUND;
    }

    if (!ecsvm_ecsbin_read_exact(file, &header, sizeof(header))) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to read ecsbin header");
        fclose(file);
        return ECSVM_ERROR_ARGUMENT;
    }

    if (memcmp(header.magic, "ECSVM", 5u) != 0 ||
        header.version[0] != ECSVM_ECSBIN_VERSION_0 ||
        header.version[1] != ECSVM_ECSBIN_VERSION_1 ||
        header.version[2] != ECSVM_ECSBIN_VERSION_2) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "ecsbin header is not recognized");
        fclose(file);
        return ECSVM_ERROR_ARGUMENT;
    }

    file_size = ecsvm_ecsbin_file_size(file);
    if (file_size < sizeof(header)) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "ecsbin file is truncated");
        fclose(file);
        return ECSVM_ERROR_ARGUMENT;
    }

    module.type_ref_count = header.type_reference_count;
    module.field_ref_count = header.field_reference_count;
    module.struct_def_count = header.struct_definition_count;
    module.field_def_count = header.field_definition_count;
    module.attribute_count = header.attribute_count;
    module.blob_count = header.blob_count;

    if (module.type_ref_count > 0u) {
        module.type_refs = (ecsvm_ecsbin_type_ref_t *)calloc(
            module.type_ref_count,
            sizeof(*module.type_refs)
        );
    }
    if (module.field_ref_count > 0u) {
        module.field_refs = (ecsvm_ecsbin_field_ref_t *)calloc(
            module.field_ref_count,
            sizeof(*module.field_refs)
        );
    }
    if (module.struct_def_count > 0u) {
        module.struct_defs = (ecsvm_ecsbin_struct_def_t *)calloc(
            module.struct_def_count,
            sizeof(*module.struct_defs)
        );
    }
    if (module.field_def_count > 0u) {
        module.field_defs = (ecsvm_ecsbin_field_def_t *)calloc(
            module.field_def_count,
            sizeof(*module.field_defs)
        );
    }
    if (module.attribute_count > 0u) {
        module.attributes = (ecsvm_ecsbin_attribute_t *)calloc(
            module.attribute_count,
            sizeof(*module.attributes)
        );
    }
    if (module.blob_count > 0u) {
        module.blobs = (ecsvm_ecsbin_blob_t *)calloc(module.blob_count, sizeof(*module.blobs));
    }

    if ((module.type_ref_count > 0u && module.type_refs == NULL) ||
        (module.field_ref_count > 0u && module.field_refs == NULL) ||
        (module.struct_def_count > 0u && module.struct_defs == NULL) ||
        (module.field_def_count > 0u && module.field_defs == NULL) ||
        (module.attribute_count > 0u && module.attributes == NULL) ||
        (module.blob_count > 0u && module.blobs == NULL)) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while allocating ecsbin tables");
        fclose(file);
        ecsvm_ecsbin_unload(&module);
        return ECSVM_ERROR_MEMORY;
    }

    blob_data_offset = header.blob_offset + (module.blob_count * sizeof(ecsvm_ecsbin_blob_disk_t));
    if (blob_data_offset > file_size) {
        ecsvm_ecsbin_set_error(error_message, error_message_capacity, "blob data offset is out of bounds");
        fclose(file);
        ecsvm_ecsbin_unload(&module);
        return ECSVM_ERROR_ARGUMENT;
    }

    if (module.blob_count > 0u) {
        ecsvm_ecsbin_blob_disk_t *disk_blobs;

        disk_blobs = (ecsvm_ecsbin_blob_disk_t *)malloc(
            module.blob_count * sizeof(*disk_blobs)
        );
        if (disk_blobs == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while loading blob references");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_MEMORY;
        }

        if (!ecsvm_ecsbin_seek(file, header.blob_offset) ||
            !ecsvm_ecsbin_read_exact(
                file,
                disk_blobs,
                module.blob_count * sizeof(*disk_blobs)
            )) {
            free(disk_blobs);
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to read blob references");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_ARGUMENT;
        }

        for (index = 0u; index < module.blob_count; ++index) {
            module.blobs[index].offset = disk_blobs[index].offset;
            module.blobs[index].length = disk_blobs[index].length;
            if (module.blobs[index].offset > file_size ||
                module.blobs[index].length > file_size ||
                blob_data_offset + module.blobs[index].offset > file_size ||
                module.blobs[index].length >
                    file_size - (blob_data_offset + module.blobs[index].offset)) {
                free(disk_blobs);
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "blob range is out of bounds");
                fclose(file);
                ecsvm_ecsbin_unload(&module);
                return ECSVM_ERROR_ARGUMENT;
            }

            if (module.blobs[index].length > 0u) {
                module.blobs[index].data = (unsigned char *)malloc(
                    (size_t)module.blobs[index].length
                );
                if (module.blobs[index].data == NULL) {
                    free(disk_blobs);
                    ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while loading blob data");
                    fclose(file);
                    ecsvm_ecsbin_unload(&module);
                    return ECSVM_ERROR_MEMORY;
                }

                if (!ecsvm_ecsbin_seek(file, blob_data_offset + module.blobs[index].offset) ||
                    !ecsvm_ecsbin_read_exact(
                        file,
                        module.blobs[index].data,
                        (size_t)module.blobs[index].length
                    )) {
                    free(disk_blobs);
                    ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to read blob payload");
                    fclose(file);
                    ecsvm_ecsbin_unload(&module);
                    return ECSVM_ERROR_ARGUMENT;
                }
            }
        }

        free(disk_blobs);
    }

    if (module.type_ref_count > 0u) {
        ecsvm_ecsbin_type_ref_disk_t *disk_refs;

        disk_refs = (ecsvm_ecsbin_type_ref_disk_t *)malloc(
            module.type_ref_count * sizeof(*disk_refs)
        );
        if (disk_refs == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while loading type references");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_MEMORY;
        }

        if (!ecsvm_ecsbin_seek(file, header.type_reference_offset) ||
            !ecsvm_ecsbin_read_exact(
                file,
                disk_refs,
                module.type_ref_count * sizeof(*disk_refs)
            )) {
            free(disk_refs);
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to read type references");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_ARGUMENT;
        }

        for (index = 0u; index < module.type_ref_count; ++index) {
            char *namespace_name;
            char *name;
            size_t namespace_length;
            size_t name_length;

            namespace_name = ecsvm_ecsbin_blob_string(&module, disk_refs[index].namespace_blob_id);
            name = ecsvm_ecsbin_blob_string(&module, disk_refs[index].name_blob_id);
            if (namespace_name == NULL || name == NULL) {
                free(namespace_name);
                free(name);
                free(disk_refs);
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "type reference uses an invalid blob");
                fclose(file);
                ecsvm_ecsbin_unload(&module);
                return ECSVM_ERROR_ARGUMENT;
            }

            module.type_refs[index].namespace_name = namespace_name;
            module.type_refs[index].name = name;
            namespace_length = strlen(namespace_name);
            name_length = strlen(name);
            module.type_refs[index].qualified_name = (char *)malloc(
                namespace_length + name_length + 2u
            );
            if (module.type_refs[index].qualified_name == NULL) {
                free(disk_refs);
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while composing type names");
                fclose(file);
                ecsvm_ecsbin_unload(&module);
                return ECSVM_ERROR_MEMORY;
            }

            if (namespace_length == 0u) {
                (void)snprintf(
                    module.type_refs[index].qualified_name,
                    name_length + 1u,
                    "%s",
                    name
                );
            } else {
                (void)snprintf(
                    module.type_refs[index].qualified_name,
                    namespace_length + name_length + 2u,
                    "%s.%s",
                    namespace_name,
                    name
                );
            }
        }

        free(disk_refs);
    }

    if (module.field_ref_count > 0u) {
        ecsvm_ecsbin_field_ref_disk_t *disk_refs;

        disk_refs = (ecsvm_ecsbin_field_ref_disk_t *)malloc(
            module.field_ref_count * sizeof(*disk_refs)
        );
        if (disk_refs == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while loading field references");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_MEMORY;
        }

        if (!ecsvm_ecsbin_seek(file, header.field_reference_offset) ||
            !ecsvm_ecsbin_read_exact(
                file,
                disk_refs,
                module.field_ref_count * sizeof(*disk_refs)
            )) {
            free(disk_refs);
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to read field references");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_ARGUMENT;
        }

        for (index = 0u; index < module.field_ref_count; ++index) {
            module.field_refs[index].name = ecsvm_ecsbin_blob_string(
                &module,
                disk_refs[index].name_blob_id
            );
            module.field_refs[index].type_id = disk_refs[index].type_id;
            if (module.field_refs[index].name == NULL ||
                module.field_refs[index].type_id == 0u ||
                module.field_refs[index].type_id > module.type_ref_count) {
                free(disk_refs);
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "field reference is invalid");
                fclose(file);
                ecsvm_ecsbin_unload(&module);
                return ECSVM_ERROR_ARGUMENT;
            }
        }

        free(disk_refs);
    }

    if (module.struct_def_count > 0u) {
        ecsvm_ecsbin_struct_def_disk_t *disk_defs;

        disk_defs = (ecsvm_ecsbin_struct_def_disk_t *)malloc(
            module.struct_def_count * sizeof(*disk_defs)
        );
        if (disk_defs == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while loading struct definitions");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_MEMORY;
        }

        if (!ecsvm_ecsbin_seek(file, header.struct_definition_offset) ||
            !ecsvm_ecsbin_read_exact(
                file,
                disk_defs,
                module.struct_def_count * sizeof(*disk_defs)
            )) {
            free(disk_defs);
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to read struct definitions");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_ARGUMENT;
        }

        for (index = 0u; index < module.struct_def_count; ++index) {
            module.struct_defs[index].type_id = disk_defs[index].type_id;
            module.struct_defs[index].flags = disk_defs[index].flags;
            module.struct_defs[index].field_start = disk_defs[index].field_start;
            module.struct_defs[index].field_count = disk_defs[index].field_count;
            module.struct_defs[index].attribute_start = disk_defs[index].attribute_start;
            module.struct_defs[index].attribute_count = disk_defs[index].attribute_count;
            if (module.struct_defs[index].type_id == 0u ||
                module.struct_defs[index].type_id > module.type_ref_count) {
                free(disk_defs);
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "struct definition has an invalid type id");
                fclose(file);
                ecsvm_ecsbin_unload(&module);
                return ECSVM_ERROR_ARGUMENT;
            }
        }

        free(disk_defs);
    }

    if (module.field_def_count > 0u) {
        ecsvm_ecsbin_field_def_disk_t *disk_defs;

        disk_defs = (ecsvm_ecsbin_field_def_disk_t *)malloc(
            module.field_def_count * sizeof(*disk_defs)
        );
        if (disk_defs == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while loading field definitions");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_MEMORY;
        }

        if (!ecsvm_ecsbin_seek(file, header.field_definition_offset) ||
            !ecsvm_ecsbin_read_exact(
                file,
                disk_defs,
                module.field_def_count * sizeof(*disk_defs)
            )) {
            free(disk_defs);
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to read field definitions");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_ARGUMENT;
        }

        for (index = 0u; index < module.field_def_count; ++index) {
            module.field_defs[index].field_id = disk_defs[index].field_id;
            module.field_defs[index].attribute_start = disk_defs[index].attribute_start;
            module.field_defs[index].attribute_count = disk_defs[index].attribute_count;
            if (module.field_defs[index].field_id == 0u ||
                module.field_defs[index].field_id > module.field_ref_count) {
                free(disk_defs);
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "field definition has an invalid field id");
                fclose(file);
                ecsvm_ecsbin_unload(&module);
                return ECSVM_ERROR_ARGUMENT;
            }
        }

        free(disk_defs);
    }

    if (module.attribute_count > 0u) {
        ecsvm_ecsbin_attribute_disk_t *disk_attributes;

        disk_attributes = (ecsvm_ecsbin_attribute_disk_t *)malloc(
            module.attribute_count * sizeof(*disk_attributes)
        );
        if (disk_attributes == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while loading attributes");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_MEMORY;
        }

        if (!ecsvm_ecsbin_seek(file, header.attribute_offset) ||
            !ecsvm_ecsbin_read_exact(
                file,
                disk_attributes,
                module.attribute_count * sizeof(*disk_attributes)
            )) {
            free(disk_attributes);
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "failed to read attributes");
            fclose(file);
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_ARGUMENT;
        }

        for (index = 0u; index < module.attribute_count; ++index) {
            module.attributes[index].type_id = disk_attributes[index].type_id;
            module.attributes[index].data = ecsvm_ecsbin_blob_string(
                &module,
                disk_attributes[index].data_blob_id
            );
            if (module.attributes[index].type_id == 0u ||
                module.attributes[index].type_id > module.type_ref_count ||
                module.attributes[index].data == NULL) {
                free(disk_attributes);
                ecsvm_ecsbin_set_error(error_message, error_message_capacity, "attribute is invalid");
                fclose(file);
                ecsvm_ecsbin_unload(&module);
                return ECSVM_ERROR_ARGUMENT;
            }
        }

        free(disk_attributes);
    }

    fclose(file);

    visit_state = NULL;
    if (module.struct_def_count > 0u) {
        visit_state = (unsigned char *)calloc(module.struct_def_count, sizeof(*visit_state));
        if (visit_state == NULL) {
            ecsvm_ecsbin_set_error(error_message, error_message_capacity, "out of memory while computing struct layouts");
            ecsvm_ecsbin_unload(&module);
            return ECSVM_ERROR_MEMORY;
        }
    }

    status = ECSVM_OK;
    for (index = 0u; index < module.struct_def_count; ++index) {
        status = ecsvm_ecsbin_compute_struct_layout(
            &module,
            index,
            visit_state,
            error_message,
            error_message_capacity
        );
        if (status != ECSVM_OK) {
            free(visit_state);
            ecsvm_ecsbin_unload(&module);
            return status;
        }
    }

    free(visit_state);
    *out_module = module;
    return ECSVM_OK;
}
