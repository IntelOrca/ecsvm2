#ifndef ECSVM_PROJECT_H
#define ECSVM_PROJECT_H

#include "ecsvm/diagnostic.h"
#include "ecsvm/ecsvm.h"
#include "ecsvm/logger.h"

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

ecsvm_status_t ecsvm_project_build_ex(
    const char *project_path,
    char *out_ecsbin_path,
    size_t out_ecsbin_path_capacity,
    char *error_message,
    size_t error_message_capacity,
    const ecsvm_logger_t *logger,
    ecsvm_diagnostic_t *diagnostic
);

#ifdef __cplusplus
}
#endif

#endif
