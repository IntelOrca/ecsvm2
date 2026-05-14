#include "ecsvm/diagnostic.h"

#include <stdio.h>
#include <string.h>

void ecsvm_diagnostic_clear(ecsvm_diagnostic_t *diagnostic)
{
    if (diagnostic == NULL) {
        return;
    }

    memset(diagnostic, 0, sizeof(*diagnostic));
}

void ecsvm_diagnostic_set(
    ecsvm_diagnostic_t *diagnostic,
    const char *file,
    size_t line,
    size_t column,
    ecsvm_diagnostic_code_t code,
    const char *message
)
{
    if (diagnostic == NULL) {
        return;
    }

    ecsvm_diagnostic_clear(diagnostic);
    if (file != NULL) {
        (void)snprintf(diagnostic->file, sizeof(diagnostic->file), "%s", file);
    }
    if (message != NULL) {
        (void)snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", message);
    }
    diagnostic->line = line;
    diagnostic->column = column;
    diagnostic->code = code;
}

const char *ecsvm_diagnostic_code_string(ecsvm_diagnostic_code_t code)
{
    switch (code) {
    case ECSVM_DIAGNOSTIC_NONE:
        return "none";
    case ECSVM_DIAGNOSTIC_ARGUMENT:
        return "argument";
    case ECSVM_DIAGNOSTIC_IO:
        return "io";
    case ECSVM_DIAGNOSTIC_OUT_OF_MEMORY:
        return "out-of-memory";
    case ECSVM_DIAGNOSTIC_UNEXPECTED_CHARACTER:
        return "unexpected-character";
    case ECSVM_DIAGNOSTIC_UNTERMINATED_COMMENT:
        return "unterminated-comment";
    case ECSVM_DIAGNOSTIC_UNTERMINATED_STRING:
        return "unterminated-string";
    case ECSVM_DIAGNOSTIC_UNEXPECTED_TOKEN:
        return "unexpected-token";
    case ECSVM_DIAGNOSTIC_INVALID_BINARY:
        return "invalid-binary";
    case ECSVM_DIAGNOSTIC_SEMANTIC:
        return "semantic";
    }

    return "unknown";
}

int ecsvm_diagnostic_format(
    const ecsvm_diagnostic_t *diagnostic,
    char *buffer,
    size_t buffer_capacity
)
{
    if (buffer == NULL || buffer_capacity == 0u) {
        return 0;
    }

    if (diagnostic == NULL ||
        (diagnostic->file[0] == '\0' && diagnostic->message[0] == '\0' &&
         diagnostic->code == ECSVM_DIAGNOSTIC_NONE)) {
        buffer[0] = '\0';
        return 1;
    }

    if (diagnostic->file[0] != '\0' && diagnostic->line > 0u && diagnostic->column > 0u) {
        return snprintf(
                   buffer,
                   buffer_capacity,
                   "%s:%zu:%zu: [%s] %s",
                   diagnostic->file,
                   diagnostic->line,
                   diagnostic->column,
                   ecsvm_diagnostic_code_string(diagnostic->code),
                   diagnostic->message
               ) > 0;
    }

    if (diagnostic->file[0] != '\0') {
        return snprintf(
                   buffer,
                   buffer_capacity,
                   "%s: [%s] %s",
                   diagnostic->file,
                   ecsvm_diagnostic_code_string(diagnostic->code),
                   diagnostic->message
               ) > 0;
    }

    return snprintf(
               buffer,
               buffer_capacity,
               "[%s] %s",
               ecsvm_diagnostic_code_string(diagnostic->code),
               diagnostic->message
           ) > 0;
}
