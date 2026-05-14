
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


ecsvm_status_t ecsvm_project_build(
    const char *project_path,
    char *out_ecsbin_path,
    size_t out_ecsbin_path_capacity,
    char *error_message,
    size_t error_message_capacity
)
{
    return ecsvm_project_build_ex(
        project_path,
        out_ecsbin_path,
        out_ecsbin_path_capacity,
        error_message,
        error_message_capacity,
        NULL,
        NULL
    );
}
