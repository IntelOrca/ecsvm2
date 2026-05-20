#ifndef ECSVM_PATH_UTIL_H
#define ECSVM_PATH_UTIL_H

#include <stddef.h>

#ifndef _WIN32
#include <limits.h>
#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#endif

int ecsvm_path_is_absolute(const char *path);
char *ecsvm_path_make_absolute(const char *path);
int ecsvm_path_is_directory(const char *path);
int ecsvm_path_exists(const char *path);
int ecsvm_path_has_extension(const char *path, const char *extension);
int ecsvm_path_join(const char *left, const char *right, char *buffer, size_t buffer_capacity);
int ecsvm_path_parent(const char *path, char *buffer, size_t buffer_capacity);
int ecsvm_ensure_directory(const char *path);

#endif
