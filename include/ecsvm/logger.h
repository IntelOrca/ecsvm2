#ifndef ECSVM_LOGGER_H
#define ECSVM_LOGGER_H

#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ecsvm_log_level {
    ECSVM_LOG_LEVEL_ERROR = 0,
    ECSVM_LOG_LEVEL_WARNING,
    ECSVM_LOG_LEVEL_INFO,
    ECSVM_LOG_LEVEL_DEBUG
} ecsvm_log_level_t;

typedef struct ecsvm_logger {
    void *userdata;
    void (*log)(void *userdata, ecsvm_log_level_t level, const char *message);
    int (*is_level)(void *userdata, ecsvm_log_level_t level);
} ecsvm_logger_t;

typedef struct ecsvm_stdio_logger {
    FILE *out;
    FILE *err;
    ecsvm_log_level_t level;
} ecsvm_stdio_logger_t;

void ecsvm_stdio_logger_init(
    ecsvm_logger_t *logger,
    ecsvm_stdio_logger_t *state,
    FILE *out,
    FILE *err,
    ecsvm_log_level_t level
);
int ecsvm_logger_is_level(const ecsvm_logger_t *logger, ecsvm_log_level_t level);
void ecsvm_logger_vlog(
    const ecsvm_logger_t *logger,
    ecsvm_log_level_t level,
    const char *format,
    va_list args
);
void ecsvm_logger_log(
    const ecsvm_logger_t *logger,
    ecsvm_log_level_t level,
    const char *format,
    ...
);
const char *ecsvm_log_level_string(ecsvm_log_level_t level);
int ecsvm_log_level_parse(const char *text, ecsvm_log_level_t *out_level);

#ifdef __cplusplus
}
#endif

#endif
