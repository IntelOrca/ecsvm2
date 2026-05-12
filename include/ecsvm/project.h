#ifndef ECSVM_PROJECT_H
#define ECSVM_PROJECT_H

#include "ecsvm/ecsvm.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int ecsvm_path_is_directory(const char *path);
int ecsvm_path_has_extension(const char *path, const char *extension);

ecsvm_status_t ecsvm_project_build(
    const char *project_path,
    char *out_ecsbin_path,
    size_t out_ecsbin_path_capacity,
    char *error_message,
    size_t error_message_capacity
);

#ifdef __cplusplus
}
#endif

#endif
