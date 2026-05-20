#include "path_util.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#define ecsvm_path_stricmp _stricmp
#else
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define ecsvm_path_stricmp strcasecmp
#endif

#include "utility.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ecsvm_path_is_absolute(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

#ifdef _WIN32
    return (path[0] >= 'A' && path[0] <= 'Z' && path[1] == ':') ||
        (path[0] >= 'a' && path[0] <= 'z' && path[1] == ':') ||
        path[0] == '\\' ||
        path[0] == '/';
#else
    return path[0] == '/';
#endif
}

char *ecsvm_path_make_absolute(const char *path)
{
    char cwd[MAX_PATH];
    size_t length;
    char *result;

    if (path == NULL) {
        return NULL;
    }

    if (ecsvm_path_is_absolute(path)) {
        return ecsvm_copy_string(path);
    }

#ifdef _WIN32
    if (_getcwd(cwd, (int)sizeof(cwd)) == NULL) {
#else
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
#endif
        return ecsvm_copy_string(path);
    }

    length = strlen(cwd) + 1u + strlen(path) + 1u;
    result = (char *)malloc(length);
    if (result == NULL) {
        return NULL;
    }

    if (!ecsvm_path_join(cwd, path, result, length)) {
        free(result);
        return NULL;
    }
    return result;
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

int ecsvm_path_exists(const char *path)
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
    return dot != NULL && ecsvm_path_stricmp(dot, extension) == 0;
}

int ecsvm_path_join(
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

#ifdef _WIN32
    return snprintf(buffer, buffer_capacity, "%s\\%s", left, right_part) > 0;
#else
    return snprintf(buffer, buffer_capacity, "%s/%s", left, right_part) > 0;
#endif
}

int ecsvm_path_parent(const char *path, char *buffer, size_t buffer_capacity)
{
    size_t length;

    if (path == NULL || buffer == NULL || buffer_capacity == 0u) {
        return 0;
    }

    length = strlen(path);
    if (length + 1u > buffer_capacity) {
        return 0;
    }

    memcpy(buffer, path, length + 1u);
    while (length > 0u &&
           (buffer[length - 1u] == '\\' || buffer[length - 1u] == '/')) {
        buffer[length - 1u] = '\0';
        length -= 1u;
    }
    while (length > 0u &&
           buffer[length - 1u] != '\\' &&
           buffer[length - 1u] != '/') {
        length -= 1u;
    }
    if (length == 0u) {
        return 0;
    }

    while (length > 0u &&
           (buffer[length - 1u] == '\\' || buffer[length - 1u] == '/')) {
        length -= 1u;
    }
    buffer[length] = '\0';
    return length > 0u;
}

int ecsvm_ensure_directory(const char *path)
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
