
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#define ECSVM_PATH_SEPARATOR '\\'
#define ecsvm_stricmp _stricmp
#else
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define ECSVM_PATH_SEPARATOR '/'
#define ecsvm_stricmp strcasecmp
#endif

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ecsvm/ecsbin.h"

#define ECSVM_ALIGNOF(type) offsetof(struct { char pad; type value; }, value)

#include "project_internal.h"

static void ecsvm_manifest_free(ecsvm_manifest_t *manifest)
{
    free(manifest->name);
    free(manifest->version);
    free(manifest->entry);
    memset(manifest, 0, sizeof(*manifest));
}

int ecsvm_path_is_directory(const char *path)
{
#ifdef _WIN32
    DWORD attributes;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat status;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    return stat(path, &status) == 0 && S_ISDIR(status.st_mode);
#endif
}

static int ecsvm_path_exists(const char *path)
{
#ifdef _WIN32
    DWORD attributes;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES;
#else
    struct stat status;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    return stat(path, &status) == 0;
#endif
}

int ecsvm_path_has_extension(const char *path, const char *extension)
{
    const char *dot;

    if (path == NULL || extension == NULL) {
        return 0;
    }

    dot = strrchr(path, '.');
    return dot != NULL && ecsvm_stricmp(dot, extension) == 0;
}

static int ecsvm_path_join(
    const char *left,
    const char *right,
    char *buffer,
    size_t buffer_capacity
)
{
    size_t left_length;
    const char *right_part;

    if (left == NULL || right == NULL || buffer == NULL || buffer_capacity == 0u) {
        return 0;
    }

    left_length = strlen(left);
    right_part = right;
    while (*right_part == '\\' || *right_part == '/') {
        right_part += 1;
    }

    if (left_length > 0u &&
        (left[left_length - 1u] == '\\' || left[left_length - 1u] == '/')) {
        return snprintf(buffer, buffer_capacity, "%s%s", left, right_part) > 0;
    }

    return snprintf(buffer, buffer_capacity, "%s%c%s", left, ECSVM_PATH_SEPARATOR, right_part) > 0;
}

static int ecsvm_read_text_file(const char *path, char **out_text, size_t *out_length)
{
    FILE *file;
    long length;
    char *text;

    file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }

    length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }

    text = (char *)malloc((size_t)length + 1u);
    if (text == NULL) {
        fclose(file);
        return 0;
    }

    if (length > 0 && fread(text, 1u, (size_t)length, file) != (size_t)length) {
        free(text);
        fclose(file);
        return 0;
    }

    fclose(file);
    text[length] = '\0';
    *out_text = text;
    if (out_length != NULL) {
        *out_length = (size_t)length;
    }
    return 1;
}

static int ecsvm_manifest_extract_value(
    const char *text,
    const char *key,
    char **out_value
)
{
    const char *cursor;
    size_t key_length;

    key_length = strlen(key);
    cursor = text;
    while (cursor != NULL && *cursor != '\0') {
        const char *line_end;
        const char *value_start;
        const char *value_end;

        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
            cursor += 1;
        }

        if (*cursor == '#') {
            line_end = strchr(cursor, '\n');
            cursor = line_end == NULL ? cursor + strlen(cursor) : line_end + 1;
            continue;
        }

        if (strncmp(cursor, key, key_length) != 0) {
            line_end = strchr(cursor, '\n');
            cursor = line_end == NULL ? cursor + strlen(cursor) : line_end + 1;
            continue;
        }

        cursor += key_length;
        while (*cursor == ' ' || *cursor == '\t') {
            cursor += 1;
        }
        if (*cursor != '=') {
            return 0;
        }

        cursor += 1;
        while (*cursor == ' ' || *cursor == '\t') {
            cursor += 1;
        }
        if (*cursor != '"') {
            return 0;
        }

        value_start = cursor + 1;
        value_end = strchr(value_start, '"');
        if (value_end == NULL) {
            return 0;
        }

        *out_value = ecsvm_copy_string_range(value_start, (size_t)(value_end - value_start));
        return *out_value != NULL;
    }

    return 0;
}

static int ecsvm_parse_manifest(
    const char *project_path,
    ecsvm_manifest_t *manifest,
    char *error_message,
    size_t error_message_capacity
)
{
    char manifest_path[MAX_PATH];
    char *text;

    if (!ecsvm_path_join(project_path, "project.toml", manifest_path, sizeof(manifest_path))) {
        ecsvm_set_error(error_message, error_message_capacity, "project.toml path is too long");
        return 0;
    }

    text = NULL;
    if (!ecsvm_read_text_file(manifest_path, &text, NULL)) {
        ecsvm_set_error(error_message, error_message_capacity, "failed to read project.toml");
        return 0;
    }

    if (!ecsvm_manifest_extract_value(text, "name", &manifest->name) ||
        !ecsvm_manifest_extract_value(text, "version", &manifest->version) ||
        !ecsvm_manifest_extract_value(text, "entry", &manifest->entry)) {
        free(text);
        ecsvm_manifest_free(manifest);
        ecsvm_set_error(error_message, error_message_capacity, "project.toml must define name, version, and entry");
        return 0;
    }

    free(text);
    return 1;
}

static int ecsvm_collect_ecs_files_recursive(
    const char *directory,
    ecsvm_string_array_t *paths,
    char *error_message,
    size_t error_message_capacity
)
{
#ifdef _WIN32
    char search_pattern[MAX_PATH];
    WIN32_FIND_DATAA find_data;
    HANDLE handle;

    if (!ecsvm_path_join(directory, "*", search_pattern, sizeof(search_pattern))) {
        ecsvm_set_error(error_message, error_message_capacity, "source path is too long");
        return 0;
    }

    handle = FindFirstFileA(search_pattern, &find_data);
    if (handle == INVALID_HANDLE_VALUE) {
        return 1;
    }

    do {
        char path[MAX_PATH];

        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        if (!ecsvm_path_join(directory, find_data.cFileName, path, sizeof(path))) {
            FindClose(handle);
            ecsvm_set_error(error_message, error_message_capacity, "source path is too long");
            return 0;
        }

        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if (!ecsvm_collect_ecs_files_recursive(path, paths, error_message, error_message_capacity)) {
                FindClose(handle);
                return 0;
            }
        } else if (ecsvm_path_has_extension(path, ".ecs")) {
            char *copy;

            copy = ecsvm_copy_string(path);
            if (copy == NULL || !ecsvm_string_array_push(paths, copy)) {
                free(copy);
                FindClose(handle);
                ecsvm_set_error(error_message, error_message_capacity, "out of memory while collecting source files");
                return 0;
            }
        }
    } while (FindNextFileA(handle, &find_data));

    FindClose(handle);
    return 1;
#else
    DIR *dir;
    struct dirent *entry;

    dir = opendir(directory);
    if (dir == NULL) {
        return 1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char path[MAX_PATH];
        struct stat status;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (!ecsvm_path_join(directory, entry->d_name, path, sizeof(path))) {
            closedir(dir);
            ecsvm_set_error(error_message, error_message_capacity, "source path is too long");
            return 0;
        }

        if (stat(path, &status) != 0) {
            continue;
        }

        if (S_ISDIR(status.st_mode)) {
            if (!ecsvm_collect_ecs_files_recursive(path, paths, error_message, error_message_capacity)) {
                closedir(dir);
                return 0;
            }
        } else if (ecsvm_path_has_extension(path, ".ecs")) {
            char *copy;

            copy = ecsvm_copy_string(path);
            if (copy == NULL || !ecsvm_string_array_push(paths, copy)) {
                free(copy);
                closedir(dir);
                ecsvm_set_error(error_message, error_message_capacity, "out of memory while collecting source files");
                return 0;
            }
        }
    }

    closedir(dir);
    return 1;
#endif
}

static int ecsvm_compare_strings(const void *left, const void *right)
{
    const char *const *left_text;
    const char *const *right_text;

    left_text = (const char *const *)left;
    right_text = (const char *const *)right;
    return ecsvm_stricmp(*left_text, *right_text);
}

static int ecsvm_ensure_directory(const char *path)
{
#ifdef _WIN32
    if (_mkdir(path) == 0 || errno == EEXIST) {
#else
    if (mkdir(path, 0777) == 0 || errno == EEXIST) {
#endif
        return 1;
    }

    return 0;
}

static int ecsvm_project_find_semantic_struct(
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

static int ecsvm_project_find_semantic_function(
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

static char *ecsvm_import_attribute_data(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_attribute_t *attribute
);

static int ecsvm_import_struct_attributes(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_struct_def_t *definition,
    ecsvm_semantic_struct_t *semantic_struct,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t attribute_index;

    for (attribute_index = 0u; attribute_index < definition->attribute_count; ++attribute_index) {
        const ecsvm_ecsbin_attribute_t *attribute;
        const ecsvm_ecsbin_type_ref_t *type_ref;
        char *attribute_name;
        char *attribute_data;

        attribute = ecsvm_ecsbin_attribute_ref(module, definition->attribute_start + (uint32_t)attribute_index);
        type_ref = attribute != NULL ? ecsvm_ecsbin_type_ref(module, attribute->type_id) : NULL;
        if (type_ref == NULL) {
            ecsvm_set_error(error_message, error_message_capacity, "core library attribute is invalid");
            return 0;
        }

        attribute_name = ecsvm_copy_string(type_ref->qualified_name);
        attribute_data = ecsvm_import_attribute_data(module, attribute);
        if (attribute_name == NULL ||
            (ecsvm_ecsbin_attribute_expects_type_payload(module, attribute) && attribute_data == NULL) ||
            !ecsvm_semantic_attribute_push(semantic_struct, attribute_name, attribute_data)) {
            free(attribute_name);
            free(attribute_data);
            ecsvm_set_error(error_message, error_message_capacity, "out of memory while importing core library attributes");
            return 0;
        }

        if (strcmp(attribute_name, "core.Component") == 0) {
            semantic_struct->is_component = 1;
        } else if (strcmp(attribute_name, "core.Attribute") == 0) {
            semantic_struct->is_attribute = 1;
        }
    }

    return 1;
}

static char *ecsvm_import_attribute_data(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_attribute_t *attribute
)
{
    uint32_t payload_type_id;
    const ecsvm_ecsbin_type_ref_t *payload_type;

    if (attribute == NULL) {
        return NULL;
    }

    if (ecsvm_ecsbin_attribute_expects_type_payload(module, attribute)) {
        if (!ecsvm_ecsbin_attribute_type_payload(module, attribute, &payload_type_id)) {
            return NULL;
        }

        payload_type = ecsvm_ecsbin_type_ref(module, payload_type_id);
        return payload_type != NULL && payload_type->qualified_name != NULL
            ? ecsvm_copy_string(payload_type->qualified_name)
            : NULL;
    }

    return attribute->data != NULL && attribute->data[0] != '\0'
        ? ecsvm_copy_string(attribute->data)
        : NULL;
}

static int ecsvm_import_function_attributes(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_function_ref_t *function_ref,
    ecsvm_semantic_function_t *semantic_function,
    char *error_message,
    size_t error_message_capacity
)
{
    size_t attribute_index;

    if (function_ref->attribute_count == 0u) {
        return 1;
    }

    /* Function attribute slot 0 stores the return type. */
    for (attribute_index = 1u; attribute_index < function_ref->attribute_count; ++attribute_index) {
        const ecsvm_ecsbin_attribute_t *attribute;
        const ecsvm_ecsbin_type_ref_t *type_ref;
        char *attribute_name;
        char *attribute_data;

        attribute = ecsvm_ecsbin_attribute_ref(module, function_ref->attribute_start + (uint32_t)attribute_index);
        type_ref = attribute != NULL ? ecsvm_ecsbin_type_ref(module, attribute->type_id) : NULL;
        if (type_ref == NULL || type_ref->qualified_name == NULL) {
            ecsvm_set_error(error_message, error_message_capacity, "core library function attribute is invalid");
            return 0;
        }

        attribute_name = ecsvm_copy_string(type_ref->qualified_name);
        attribute_data = ecsvm_import_attribute_data(module, attribute);
        if (attribute_name == NULL ||
            (ecsvm_ecsbin_attribute_expects_type_payload(module, attribute) && attribute_data == NULL) ||
            !ecsvm_semantic_function_attribute_push(semantic_function, attribute_name, attribute_data)) {
            free(attribute_name);
            free(attribute_data);
            ecsvm_set_error(error_message, error_message_capacity, "out of memory while importing core library function attributes");
            return 0;
        }
    }

    return 1;
}

static int ecsvm_import_core_library(
    const char *core_library_path,
    ecsvm_semantic_struct_array_t *semantic_structs,
    ecsvm_semantic_function_array_t *semantic_functions,
    char *error_message,
    size_t error_message_capacity,
    ecsvm_diagnostic_t *diagnostic
)
{
    ecsvm_ecsbin_module_t module;
    ecsvm_status_t status;
    size_t index;

    if (core_library_path == NULL || core_library_path[0] == '\0') {
        return 1;
    }

    memset(&module, 0, sizeof(module));
    status = ecsvm_ecsbin_load_ex(
        core_library_path,
        &module,
        error_message,
        error_message_capacity,
        diagnostic
    );
    if (status != ECSVM_OK) {
        return 0;
    }

    for (index = 0u; index < module.struct_def_count; ++index) {
        const ecsvm_ecsbin_struct_def_t *definition;
        const ecsvm_ecsbin_type_ref_t *type_ref;
        ecsvm_semantic_struct_t semantic_struct;
        size_t field_index;

        definition = &module.struct_defs[index];
        type_ref = ecsvm_ecsbin_type_ref(&module, definition->type_id);
        if (type_ref == NULL || type_ref->qualified_name == NULL) {
            ecsvm_set_error(error_message, error_message_capacity, "core library type is invalid");
            ecsvm_ecsbin_unload(&module);
            return 0;
        }

        if (ecsvm_project_find_semantic_struct(semantic_structs, type_ref->qualified_name) >= 0) {
            continue;
        }

        memset(&semantic_struct, 0, sizeof(semantic_struct));
        semantic_struct.namespace_name = ecsvm_copy_string(type_ref->namespace_name);
        semantic_struct.name = ecsvm_copy_string(type_ref->name);
        semantic_struct.qualified_name = ecsvm_copy_string(type_ref->qualified_name);
        semantic_struct.is_component = ecsvm_ecsbin_struct_is_component(&module, definition);
        semantic_struct.size = definition->size;
        semantic_struct.alignment = definition->alignment;
        semantic_struct.layout_state = definition->size == 0u ? 0 : 2;
        semantic_struct.emit_state = 0;
        if (semantic_struct.namespace_name == NULL ||
            semantic_struct.name == NULL ||
            semantic_struct.qualified_name == NULL) {
            ecsvm_semantic_struct_free(&semantic_struct);
            ecsvm_set_error(error_message, error_message_capacity, "out of memory while importing core library types");
            ecsvm_ecsbin_unload(&module);
            return 0;
        }

        if (!ecsvm_import_struct_attributes(
                &module,
                definition,
                &semantic_struct,
                error_message,
                error_message_capacity
            )) {
            ecsvm_semantic_struct_free(&semantic_struct);
            ecsvm_ecsbin_unload(&module);
            return 0;
        }

        for (field_index = 0u; field_index < definition->field_count; ++field_index) {
            const ecsvm_ecsbin_field_ref_t *field_ref;
            const ecsvm_ecsbin_type_ref_t *field_type;
            ecsvm_semantic_field_t field;

            field_ref = definition->field_start == 0u
                ? NULL
                : &module.field_refs[definition->field_start - 1u + field_index];
            field_type = field_ref != NULL ? ecsvm_ecsbin_type_ref(&module, field_ref->type_id) : NULL;
            if (field_ref == NULL || field_type == NULL) {
                ecsvm_semantic_struct_free(&semantic_struct);
                ecsvm_set_error(error_message, error_message_capacity, "core library field is invalid");
                ecsvm_ecsbin_unload(&module);
                return 0;
            }

            memset(&field, 0, sizeof(field));
            field.name = ecsvm_copy_string(field_ref->name);
            field.type_name = ecsvm_copy_string(field_type->qualified_name);
            if (field.name == NULL || field.type_name == NULL ||
                !ecsvm_semantic_field_array_push(&semantic_struct, field)) {
                free(field.name);
                free(field.type_name);
                ecsvm_semantic_struct_free(&semantic_struct);
                ecsvm_set_error(error_message, error_message_capacity, "out of memory while importing core library fields");
                ecsvm_ecsbin_unload(&module);
                return 0;
            }
        }

        if (!ecsvm_semantic_struct_array_push(semantic_structs, semantic_struct)) {
            ecsvm_semantic_struct_free(&semantic_struct);
            ecsvm_set_error(error_message, error_message_capacity, "out of memory while importing core library structs");
            ecsvm_ecsbin_unload(&module);
            return 0;
        }
    }

    for (index = 0u; index < module.function_ref_count; ++index) {
        const ecsvm_ecsbin_function_ref_t *function_ref;
        const ecsvm_ecsbin_type_ref_t *return_type;
        ecsvm_semantic_function_t semantic_function;
        size_t parameter_index;

        function_ref = &module.function_refs[index];
        if (ecsvm_project_find_semantic_function(semantic_functions, function_ref->qualified_name) >= 0) {
            continue;
        }

        return_type = ecsvm_ecsbin_function_return_type(&module, function_ref);
        if (return_type == NULL) {
            ecsvm_set_error(error_message, error_message_capacity, "core library function return type is invalid");
            ecsvm_ecsbin_unload(&module);
            return 0;
        }

        memset(&semantic_function, 0, sizeof(semantic_function));
        semantic_function.namespace_name = ecsvm_copy_string(function_ref->namespace_name);
        semantic_function.name = ecsvm_copy_string(function_ref->name);
        semantic_function.qualified_name = ecsvm_copy_string(function_ref->qualified_name);
        semantic_function.return_type_name = ecsvm_copy_string(return_type->qualified_name);
        if (semantic_function.namespace_name == NULL ||
            semantic_function.name == NULL ||
            semantic_function.qualified_name == NULL ||
            semantic_function.return_type_name == NULL) {
            ecsvm_semantic_function_free(&semantic_function);
            ecsvm_set_error(error_message, error_message_capacity, "out of memory while importing core library functions");
            ecsvm_ecsbin_unload(&module);
            return 0;
        }

        for (parameter_index = 0u; parameter_index < function_ref->parameter_count; ++parameter_index) {
            const ecsvm_ecsbin_parameter_t *parameter_ref;
            const ecsvm_ecsbin_type_ref_t *parameter_type;
            const ecsvm_ecsbin_blob_t *default_value_blob;
            ecsvm_semantic_parameter_t parameter;

            parameter_ref = ecsvm_ecsbin_parameter_ref(
                &module,
                function_ref->parameter_start + (uint32_t)parameter_index
            );
            parameter_type = parameter_ref != NULL ? ecsvm_ecsbin_type_ref(&module, parameter_ref->type_id) : NULL;
            if (parameter_ref == NULL || parameter_type == NULL) {
                ecsvm_semantic_function_free(&semantic_function);
                ecsvm_set_error(error_message, error_message_capacity, "core library parameter is invalid");
                ecsvm_ecsbin_unload(&module);
                return 0;
            }

            memset(&parameter, 0, sizeof(parameter));
            default_value_blob = parameter_ref->default_value_blob_id == 0u
                ? NULL
                : ecsvm_ecsbin_blob_ref(&module, parameter_ref->default_value_blob_id);
            parameter.name = ecsvm_copy_string(parameter_ref->name);
            parameter.type_name = ecsvm_copy_string(parameter_type->qualified_name);
            parameter.default_value = default_value_blob == NULL
                ? NULL
                : (default_value_blob->data != NULL
                    ? ecsvm_copy_string_range(
                        (const char *)default_value_blob->data,
                        (size_t)default_value_blob->length
                    )
                    : ecsvm_copy_string(""));
            if (parameter.name == NULL ||
                parameter.type_name == NULL ||
                !ecsvm_semantic_function_parameter_push(&semantic_function, parameter)) {
                ecsvm_semantic_parameter_free(&parameter);
                ecsvm_semantic_function_free(&semantic_function);
                ecsvm_set_error(error_message, error_message_capacity, "out of memory while importing core library parameters");
                ecsvm_ecsbin_unload(&module);
                return 0;
            }
        }

        if (!ecsvm_import_function_attributes(
                &module,
                function_ref,
                &semantic_function,
                error_message,
                error_message_capacity
            )) {
            ecsvm_semantic_function_free(&semantic_function);
            ecsvm_ecsbin_unload(&module);
            return 0;
        }

        if (!ecsvm_semantic_function_array_push(semantic_functions, semantic_function)) {
            ecsvm_semantic_function_free(&semantic_function);
            ecsvm_set_error(error_message, error_message_capacity, "out of memory while importing core library functions");
            ecsvm_ecsbin_unload(&module);
            return 0;
        }
    }

    ecsvm_ecsbin_unload(&module);
    return 1;
}

static ecsvm_status_t ecsvm_project_build_internal(
    const char *project_path,
    const char *core_library_path,
    char *out_ecsbin_path,
    size_t out_ecsbin_path_capacity,
    char *error_message,
    size_t error_message_capacity,
    const ecsvm_logger_t *logger,
    ecsvm_diagnostic_t *diagnostic
)
{
    ecsvm_manifest_t manifest;
    ecsvm_string_array_t source_paths;
    ecsvm_source_file_array_t files;
    ecsvm_semantic_struct_array_t semantic_structs;
    ecsvm_semantic_function_array_t semantic_functions;
    ecsvm_blob_array_t blobs;
    ecsvm_type_ref_builder_array_t type_refs;
    ecsvm_field_ref_builder_array_t field_refs;
    ecsvm_field_def_builder_array_t field_defs;
    ecsvm_function_ref_builder_array_t function_refs;
    ecsvm_parameter_builder_array_t parameters;
    ecsvm_attribute_builder_array_t attributes;
    ecsvm_struct_def_builder_array_t struct_defs;
    char src_path[MAX_PATH];
    char entry_path[MAX_PATH];
    char out_path[MAX_PATH];
    char ecsbin_name[MAX_PATH];
    char ecsbin_path[MAX_PATH];
    char types_path[MAX_PATH];
    size_t source_index;
    ecsvm_status_t result;

    memset(&manifest, 0, sizeof(manifest));
    memset(&source_paths, 0, sizeof(source_paths));
    memset(&files, 0, sizeof(files));
    memset(&semantic_structs, 0, sizeof(semantic_structs));
    memset(&semantic_functions, 0, sizeof(semantic_functions));
    memset(&blobs, 0, sizeof(blobs));
    memset(&type_refs, 0, sizeof(type_refs));
    memset(&field_refs, 0, sizeof(field_refs));
    memset(&field_defs, 0, sizeof(field_defs));
    memset(&function_refs, 0, sizeof(function_refs));
    memset(&parameters, 0, sizeof(parameters));
    memset(&attributes, 0, sizeof(attributes));
    memset(&struct_defs, 0, sizeof(struct_defs));
    ecsvm_set_error(error_message, error_message_capacity, NULL);
    if (diagnostic != NULL) {
        ecsvm_diagnostic_clear(diagnostic);
    }
    ecsvm_logger_log(logger, ECSVM_LOG_LEVEL_DEBUG, "building project %s", project_path != NULL ? project_path : "<null>");

    if (project_path == NULL || !ecsvm_path_is_directory(project_path)) {
        ecsvm_set_error(error_message, error_message_capacity, "project path must be a directory");
        return ECSVM_ERROR_ARGUMENT;
    }

    if (!ecsvm_parse_manifest(project_path, &manifest, error_message, error_message_capacity)) {
        return ECSVM_ERROR_ARGUMENT;
    }

    if (!ecsvm_path_join(project_path, manifest.entry, entry_path, sizeof(entry_path)) ||
        !ecsvm_path_exists(entry_path)) {
        ecsvm_manifest_free(&manifest);
        ecsvm_set_error(error_message, error_message_capacity, "manifest entry file does not exist");
        return ECSVM_ERROR_ARGUMENT;
    }

    if (!ecsvm_path_join(project_path, "src", src_path, sizeof(src_path)) ||
        !ecsvm_path_is_directory(src_path)) {
        ecsvm_manifest_free(&manifest);
        ecsvm_set_error(error_message, error_message_capacity, "project src directory does not exist");
        return ECSVM_ERROR_ARGUMENT;
    }

    if (!ecsvm_collect_ecs_files_recursive(
            src_path,
            &source_paths,
            error_message,
            error_message_capacity
        )) {
        ecsvm_manifest_free(&manifest);
        ecsvm_string_array_free(&source_paths);
        return ECSVM_ERROR_ARGUMENT;
    }

    if (source_paths.count == 0u) {
        ecsvm_manifest_free(&manifest);
        ecsvm_string_array_free(&source_paths);
        ecsvm_set_error(error_message, error_message_capacity, "project does not contain any .ecs files");
        return ECSVM_ERROR_ARGUMENT;
    }

    qsort(source_paths.items, source_paths.count, sizeof(*source_paths.items), ecsvm_compare_strings);
    for (source_index = 0u; source_index < source_paths.count; ++source_index) {
        ecsvm_source_file_t file;

        memset(&file, 0, sizeof(file));
        file.path = source_paths.items[source_index];
        source_paths.items[source_index] = NULL;
        if (!ecsvm_read_text_file(file.path, &file.source, &file.length) ||
            !ecsvm_lex_source(&file, error_message, error_message_capacity, diagnostic) ||
            !ecsvm_parse_file(&file, error_message, error_message_capacity, diagnostic) ||
            !ecsvm_source_file_array_push(&files, file)) {
            ecsvm_source_file_free(&file);
            result = ECSVM_ERROR_ARGUMENT;
            goto cleanup;
        }
    }
    ecsvm_string_array_free(&source_paths);

    if (!ecsvm_collect_semantics(&files, &semantic_structs, &semantic_functions, error_message, error_message_capacity, diagnostic) ||
        !ecsvm_import_core_library(
            core_library_path,
            &semantic_structs,
            &semantic_functions,
            error_message,
            error_message_capacity,
            diagnostic
        ) ||
        !ecsvm_resolve_semantic_types(&semantic_structs, &semantic_functions, error_message, error_message_capacity, diagnostic) ||
        !ecsvm_compute_layouts(&semantic_structs, error_message, error_message_capacity, diagnostic)) {
        result = ECSVM_ERROR_ARGUMENT;
        goto cleanup;
    }

    if (!ecsvm_path_join(project_path, "out", out_path, sizeof(out_path)) ||
        !ecsvm_ensure_directory(out_path)) {
        ecsvm_set_error(error_message, error_message_capacity, "failed to create out directory");
        result = ECSVM_ERROR_NOT_FOUND;
        goto cleanup;
    }

    if (!ecsvm_path_join(out_path, "types.h", types_path, sizeof(types_path)) ||
        snprintf(ecsbin_name, sizeof(ecsbin_name), "%s.ecsbin", manifest.name) <= 0 ||
        !ecsvm_path_join(out_path, ecsbin_name, ecsbin_path, sizeof(ecsbin_path))) {
        ecsvm_set_error(error_message, error_message_capacity, "output path is too long");
        result = ECSVM_ERROR_ARGUMENT;
        goto cleanup;
    }

    if (!ecsvm_build_ecsbin_tables(
            &semantic_structs,
            &semantic_functions,
            &blobs,
            &type_refs,
            &field_refs,
            &field_defs,
            &function_refs,
            &parameters,
            &attributes,
            &struct_defs
        ) ||
        !ecsvm_write_ecsbin_file(
            ecsbin_path,
            &blobs,
            &type_refs,
            &field_refs,
            &field_defs,
            &function_refs,
            &parameters,
            &attributes,
            &struct_defs
        ) ||
        !ecsvm_write_types_header(types_path, &manifest, &semantic_structs)) {
        ecsvm_set_error(error_message, error_message_capacity, "failed to write project output");
        result = ECSVM_ERROR_NOT_FOUND;
        goto cleanup;
    }

    if (out_ecsbin_path != NULL && out_ecsbin_path_capacity > 0u) {
        (void)snprintf(out_ecsbin_path, out_ecsbin_path_capacity, "%s", ecsbin_path);
    }

    result = ECSVM_OK;

cleanup:
    ecsvm_manifest_free(&manifest);
    ecsvm_string_array_free(&source_paths);
    ecsvm_source_file_array_free(&files);
    ecsvm_semantic_struct_array_free(&semantic_structs);
    ecsvm_semantic_function_array_free(&semantic_functions);
    ecsvm_blob_array_free(&blobs);
    ecsvm_type_ref_builder_array_free(&type_refs);
    ecsvm_field_ref_builder_array_free(&field_refs);
    ecsvm_field_def_builder_array_free(&field_defs);
    ecsvm_function_ref_builder_array_free(&function_refs);
    ecsvm_parameter_builder_array_free(&parameters);
    ecsvm_attribute_builder_array_free(&attributes);
    ecsvm_struct_def_builder_array_free(&struct_defs);
    return result;
}

ecsvm_status_t ecsvm_project_build_with_core_ex(
    const char *project_path,
    const char *core_library_path,
    char *out_ecsbin_path,
    size_t out_ecsbin_path_capacity,
    char *error_message,
    size_t error_message_capacity,
    const ecsvm_logger_t *logger,
    ecsvm_diagnostic_t *diagnostic
)
{
    return ecsvm_project_build_internal(
        project_path,
        core_library_path,
        out_ecsbin_path,
        out_ecsbin_path_capacity,
        error_message,
        error_message_capacity,
        logger,
        diagnostic
    );
}

ecsvm_status_t ecsvm_project_build_ex(
    const char *project_path,
    char *out_ecsbin_path,
    size_t out_ecsbin_path_capacity,
    char *error_message,
    size_t error_message_capacity,
    const ecsvm_logger_t *logger,
    ecsvm_diagnostic_t *diagnostic
)
{
    return ecsvm_project_build_internal(
        project_path,
        NULL,
        out_ecsbin_path,
        out_ecsbin_path_capacity,
        error_message,
        error_message_capacity,
        logger,
        diagnostic
    );
}

ecsvm_status_t ecsvm_project_build_with_core(
    const char *project_path,
    const char *core_library_path,
    char *out_ecsbin_path,
    size_t out_ecsbin_path_capacity,
    char *error_message,
    size_t error_message_capacity
)
{
    return ecsvm_project_build_with_core_ex(
        project_path,
        core_library_path,
        out_ecsbin_path,
        out_ecsbin_path_capacity,
        error_message,
        error_message_capacity,
        NULL,
        NULL
    );
}


ecsvm_status_t ecsvm_project_build(
    const char *project_path,
    char *out_ecsbin_path,
    size_t out_ecsbin_path_capacity,
    char *error_message,
    size_t error_message_capacity
)
{
    return ecsvm_project_build_internal(
        project_path,
        NULL,
        out_ecsbin_path,
        out_ecsbin_path_capacity,
        error_message,
        error_message_capacity,
        NULL,
        NULL
    );
}
