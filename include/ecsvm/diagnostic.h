#ifndef ECSVM_DIAGNOSTIC_H
#define ECSVM_DIAGNOSTIC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ecsvm_diagnostic_code {
    ECSVM_DIAGNOSTIC_NONE = 0,
    ECSVM_DIAGNOSTIC_ARGUMENT,
    ECSVM_DIAGNOSTIC_IO,
    ECSVM_DIAGNOSTIC_OUT_OF_MEMORY,
    ECSVM_DIAGNOSTIC_UNEXPECTED_CHARACTER,
    ECSVM_DIAGNOSTIC_UNTERMINATED_COMMENT,
    ECSVM_DIAGNOSTIC_UNTERMINATED_STRING,
    ECSVM_DIAGNOSTIC_UNEXPECTED_TOKEN,
    ECSVM_DIAGNOSTIC_INVALID_BINARY,
    ECSVM_DIAGNOSTIC_SEMANTIC
} ecsvm_diagnostic_code_t;

typedef struct ecsvm_diagnostic {
    char file[512];
    size_t line;
    size_t column;
    ecsvm_diagnostic_code_t code;
    char message[256];
} ecsvm_diagnostic_t;

void ecsvm_diagnostic_clear(ecsvm_diagnostic_t *diagnostic);
void ecsvm_diagnostic_set(
    ecsvm_diagnostic_t *diagnostic,
    const char *file,
    size_t line,
    size_t column,
    ecsvm_diagnostic_code_t code,
    const char *message
);
const char *ecsvm_diagnostic_code_string(ecsvm_diagnostic_code_t code);
int ecsvm_diagnostic_format(
    const ecsvm_diagnostic_t *diagnostic,
    char *buffer,
    size_t buffer_capacity
);

#ifdef __cplusplus
}
#endif

#endif
