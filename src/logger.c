#include "ecsvm/logger.h"

#include <stdio.h>
#include <string.h>

static void ecsvm_stdio_logger_log(
    void *userdata,
    ecsvm_log_level_t level,
    const char *message
)
{
    ecsvm_stdio_logger_t *state;
    FILE *stream;

    state = (ecsvm_stdio_logger_t *)userdata;
    stream = (level <= ECSVM_LOG_LEVEL_WARNING) ? state->err : state->out;
    if (stream == NULL) {
        return;
    }

    (void)fprintf(stream, "%s: %s\n", ecsvm_log_level_string(level), message);
    (void)fflush(stream);
}

static int ecsvm_stdio_logger_is_level(void *userdata, ecsvm_log_level_t level)
{
    ecsvm_stdio_logger_t *state;

    state = (ecsvm_stdio_logger_t *)userdata;
    return level <= state->level;
}

void ecsvm_stdio_logger_init(
    ecsvm_logger_t *logger,
    ecsvm_stdio_logger_t *state,
    FILE *out,
    FILE *err,
    ecsvm_log_level_t level
)
{
    if (logger == NULL || state == NULL) {
        return;
    }

    state->out = out;
    state->err = err;
    state->level = level;
    logger->userdata = state;
    logger->log = ecsvm_stdio_logger_log;
    logger->is_level = ecsvm_stdio_logger_is_level;
}

int ecsvm_logger_is_level(const ecsvm_logger_t *logger, ecsvm_log_level_t level)
{
    if (logger == NULL || logger->is_level == NULL) {
        return 0;
    }

    return logger->is_level(logger->userdata, level);
}

void ecsvm_logger_vlog(
    const ecsvm_logger_t *logger,
    ecsvm_log_level_t level,
    const char *format,
    va_list args
)
{
    char buffer[1024];

    if (logger == NULL || logger->log == NULL || !ecsvm_logger_is_level(logger, level)) {
        return;
    }

    (void)vsnprintf(buffer, sizeof(buffer), format, args);
    logger->log(logger->userdata, level, buffer);
}

void ecsvm_logger_log(
    const ecsvm_logger_t *logger,
    ecsvm_log_level_t level,
    const char *format,
    ...
)
{
    va_list args;

    va_start(args, format);
    ecsvm_logger_vlog(logger, level, format, args);
    va_end(args);
}

const char *ecsvm_log_level_string(ecsvm_log_level_t level)
{
    switch (level) {
    case ECSVM_LOG_LEVEL_ERROR:
        return "error";
    case ECSVM_LOG_LEVEL_WARNING:
        return "warning";
    case ECSVM_LOG_LEVEL_INFO:
        return "info";
    case ECSVM_LOG_LEVEL_DEBUG:
        return "debug";
    }

    return "unknown";
}

int ecsvm_log_level_parse(const char *text, ecsvm_log_level_t *out_level)
{
    if (text == NULL || out_level == NULL) {
        return 0;
    }

    if (strcmp(text, "error") == 0) {
        *out_level = ECSVM_LOG_LEVEL_ERROR;
    } else if (strcmp(text, "warning") == 0) {
        *out_level = ECSVM_LOG_LEVEL_WARNING;
    } else if (strcmp(text, "info") == 0) {
        *out_level = ECSVM_LOG_LEVEL_INFO;
    } else if (strcmp(text, "debug") == 0) {
        *out_level = ECSVM_LOG_LEVEL_DEBUG;
    } else {
        return 0;
    }

    return 1;
}
