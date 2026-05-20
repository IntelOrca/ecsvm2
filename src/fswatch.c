#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define ecsvm_fswatch_stricmp _stricmp
#define ECSVM_FSWATCH_PATH_SEPARATOR '\\'
#else
#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define ecsvm_fswatch_stricmp strcasecmp
#define ECSVM_FSWATCH_PATH_SEPARATOR '/'
#endif

#include "fswatch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

static void ecsvm_fswatch_set_error(char *error_message, size_t capacity, const char *message)
{
    if (error_message == NULL || capacity == 0u) {
        return;
    }

    if (message == NULL) {
        error_message[0] = '\0';
        return;
    }

    (void)snprintf(error_message, capacity, "%s", message);
}

static char *ecsvm_fswatch_copy_string(const char *text)
{
    char *copy;
    size_t length;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1u);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1u);
    return copy;
}

static void ecsvm_fswatch_entry_free(ecsvm_fswatch_entry_t *entry)
{
    if (entry == NULL) {
        return;
    }

    free(entry->path);
    memset(entry, 0, sizeof(*entry));
}

static void ecsvm_fswatch_entries_free(ecsvm_fswatch_entry_t *entries, size_t count)
{
    size_t index;

    if (entries == NULL) {
        return;
    }

    for (index = 0u; index < count; ++index) {
        ecsvm_fswatch_entry_free(&entries[index]);
    }
    free(entries);
}

static int ecsvm_fswatch_reserve_entries(
    ecsvm_fswatch_entry_t **entries,
    size_t *capacity,
    size_t minimum
)
{
    ecsvm_fswatch_entry_t *memory;
    size_t new_capacity;

    if (minimum <= *capacity) {
        return 1;
    }

    new_capacity = *capacity == 0u ? 8u : *capacity;
    while (new_capacity < minimum) {
        new_capacity *= 2u;
    }

    memory = (ecsvm_fswatch_entry_t *)realloc(*entries, new_capacity * sizeof(*memory));
    if (memory == NULL) {
        return 0;
    }

    *entries = memory;
    *capacity = new_capacity;
    return 1;
}

static int ecsvm_fswatch_push_entry(
    ecsvm_fswatch_entry_t **entries,
    size_t *count,
    size_t *capacity,
    const char *path,
    uint64_t modified_time,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_fswatch_entry_t *entry;
    char *path_copy;

    if (!ecsvm_fswatch_reserve_entries(entries, capacity, *count + 1u)) {
        ecsvm_fswatch_set_error(error_message, error_message_capacity, "out of memory while tracking source files");
        return 0;
    }

    path_copy = ecsvm_fswatch_copy_string(path);
    if (path_copy == NULL) {
        ecsvm_fswatch_set_error(error_message, error_message_capacity, "out of memory while tracking source files");
        return 0;
    }

    entry = &(*entries)[*count];
    memset(entry, 0, sizeof(*entry));
    entry->path = path_copy;
    entry->modified_time = modified_time;
    *count += 1u;
    return 1;
}

static int ecsvm_fswatch_path_join(
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

    return snprintf(
        buffer,
        buffer_capacity,
        "%s%c%s",
        left,
        ECSVM_FSWATCH_PATH_SEPARATOR,
        right_part
    ) > 0;
}

static int ecsvm_fswatch_has_ecs_extension(const char *path)
{
    const char *dot;

    if (path == NULL) {
        return 0;
    }

    dot = strrchr(path, '.');
    return dot != NULL && ecsvm_fswatch_stricmp(dot, ".ecs") == 0;
}

static int ecsvm_fswatch_compare_paths(const void *left, const void *right)
{
    const ecsvm_fswatch_entry_t *left_entry;
    const ecsvm_fswatch_entry_t *right_entry;

    left_entry = (const ecsvm_fswatch_entry_t *)left;
    right_entry = (const ecsvm_fswatch_entry_t *)right;
    return ecsvm_fswatch_stricmp(left_entry->path, right_entry->path);
}

static int ecsvm_fswatch_snapshot_equal(
    const ecsvm_fswatch_entry_t *left_entries,
    size_t left_count,
    const ecsvm_fswatch_entry_t *right_entries,
    size_t right_count
)
{
    size_t index;

    if (left_count != right_count) {
        return 0;
    }

    for (index = 0u; index < left_count; ++index) {
        if (ecsvm_fswatch_stricmp(left_entries[index].path, right_entries[index].path) != 0 ||
            left_entries[index].modified_time != right_entries[index].modified_time) {
            return 0;
        }
    }

    return 1;
}

#ifdef _WIN32
static uint64_t ecsvm_fswatch_filetime_value(FILETIME file_time)
{
    ULARGE_INTEGER value;

    value.LowPart = file_time.dwLowDateTime;
    value.HighPart = file_time.dwHighDateTime;
    return value.QuadPart;
}

static int ecsvm_fswatch_collect_recursive(
    const char *directory,
    ecsvm_fswatch_entry_t **entries,
    size_t *count,
    size_t *capacity,
    char *error_message,
    size_t error_message_capacity
)
{
    char search_pattern[MAX_PATH];
    WIN32_FIND_DATAA find_data;
    HANDLE handle;

    if (!ecsvm_fswatch_path_join(directory, "*", search_pattern, sizeof(search_pattern))) {
        ecsvm_fswatch_set_error(error_message, error_message_capacity, "source path is too long");
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

        if (!ecsvm_fswatch_path_join(directory, find_data.cFileName, path, sizeof(path))) {
            FindClose(handle);
            ecsvm_fswatch_set_error(error_message, error_message_capacity, "source path is too long");
            return 0;
        }

        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u) {
            if (!ecsvm_fswatch_collect_recursive(
                    path,
                    entries,
                    count,
                    capacity,
                    error_message,
                    error_message_capacity
                )) {
                FindClose(handle);
                return 0;
            }
            continue;
        }

        if (ecsvm_fswatch_has_ecs_extension(path) &&
            !ecsvm_fswatch_push_entry(
                entries,
                count,
                capacity,
                path,
                ecsvm_fswatch_filetime_value(find_data.ftLastWriteTime),
                error_message,
                error_message_capacity
            )) {
            FindClose(handle);
            return 0;
        }
    } while (FindNextFileA(handle, &find_data));

    FindClose(handle);
    return 1;
}
#else
static uint64_t ecsvm_fswatch_stat_time(const struct stat *status)
{
    return ((uint64_t)status->st_mtim.tv_sec * 1000000000ull) + (uint64_t)status->st_mtim.tv_nsec;
}

static int ecsvm_fswatch_collect_recursive(
    const char *directory,
    ecsvm_fswatch_entry_t **entries,
    size_t *count,
    size_t *capacity,
    char *error_message,
    size_t error_message_capacity
)
{
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

        if (!ecsvm_fswatch_path_join(directory, entry->d_name, path, sizeof(path))) {
            closedir(dir);
            ecsvm_fswatch_set_error(error_message, error_message_capacity, "source path is too long");
            return 0;
        }

        if (stat(path, &status) != 0) {
            continue;
        }

        if (S_ISDIR(status.st_mode)) {
            if (!ecsvm_fswatch_collect_recursive(
                    path,
                    entries,
                    count,
                    capacity,
                    error_message,
                    error_message_capacity
                )) {
                closedir(dir);
                return 0;
            }
            continue;
        }

        if (S_ISREG(status.st_mode) &&
            ecsvm_fswatch_has_ecs_extension(path) &&
            !ecsvm_fswatch_push_entry(
                entries,
                count,
                capacity,
                path,
                ecsvm_fswatch_stat_time(&status),
                error_message,
                error_message_capacity
            )) {
            closedir(dir);
            return 0;
        }
    }

    closedir(dir);
    return 1;
}
#endif

static int ecsvm_fswatch_snapshot(
    const char *root_path,
    ecsvm_fswatch_entry_t **out_entries,
    size_t *out_count,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_fswatch_entry_t *entries;
    size_t count;
    size_t capacity;

    entries = NULL;
    count = 0u;
    capacity = 0u;
    *out_entries = NULL;
    *out_count = 0u;
    ecsvm_fswatch_set_error(error_message, error_message_capacity, NULL);

    if (!ecsvm_fswatch_collect_recursive(
            root_path,
            &entries,
            &count,
            &capacity,
            error_message,
            error_message_capacity
        )) {
        ecsvm_fswatch_entries_free(entries, count);
        return 0;
    }

    if (count > 1u) {
        qsort(entries, count, sizeof(*entries), ecsvm_fswatch_compare_paths);
    }

    *out_entries = entries;
    *out_count = count;
    return 1;
}

int ecsvm_fswatch_init(
    ecsvm_fswatch_t *watch,
    const char *root_path,
    char *error_message,
    size_t error_message_capacity
)
{
    if (watch == NULL || root_path == NULL || root_path[0] == '\0') {
        ecsvm_fswatch_set_error(error_message, error_message_capacity, "invalid file watcher root path");
        return 0;
    }

    memset(watch, 0, sizeof(*watch));
    watch->root_path = ecsvm_fswatch_copy_string(root_path);
    if (watch->root_path == NULL) {
        ecsvm_fswatch_set_error(error_message, error_message_capacity, "out of memory while preparing file watcher");
        return 0;
    }

    if (!ecsvm_fswatch_snapshot(
            watch->root_path,
            &watch->entries,
            &watch->count,
            error_message,
            error_message_capacity
        )) {
        ecsvm_fswatch_free(watch);
        return 0;
    }

    watch->capacity = watch->count;
    return 1;
}

void ecsvm_fswatch_free(ecsvm_fswatch_t *watch)
{
    if (watch == NULL) {
        return;
    }

    free(watch->root_path);
    ecsvm_fswatch_entries_free(watch->entries, watch->count);
    memset(watch, 0, sizeof(*watch));
}

int ecsvm_fswatch_poll(
    ecsvm_fswatch_t *watch,
    int *out_changed,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_fswatch_entry_t *entries;
    size_t count;
    int changed;

    if (watch == NULL || out_changed == NULL) {
        ecsvm_fswatch_set_error(error_message, error_message_capacity, "invalid file watcher state");
        return 0;
    }

    entries = NULL;
    count = 0u;
    changed = 0;
    if (!ecsvm_fswatch_snapshot(
            watch->root_path,
            &entries,
            &count,
            error_message,
            error_message_capacity
        )) {
        return 0;
    }

    changed = !ecsvm_fswatch_snapshot_equal(watch->entries, watch->count, entries, count);
    if (changed) {
        ecsvm_fswatch_entries_free(watch->entries, watch->count);
        watch->entries = entries;
        watch->count = count;
        watch->capacity = count;
    } else {
        ecsvm_fswatch_entries_free(entries, count);
    }

    *out_changed = changed;
    return 1;
}
