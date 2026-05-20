#ifndef ECSVM_TEXT_BUFFER_H
#define ECSVM_TEXT_BUFFER_H

#include <stddef.h>

typedef struct ecsvm_text_buffer {
    char *data;
    size_t length;
    size_t capacity;
} ecsvm_text_buffer_t;

int ecsvm_text_buffer_reserve(ecsvm_text_buffer_t *buffer, size_t additional);
int ecsvm_text_buffer_append_range(ecsvm_text_buffer_t *buffer, const char *text, size_t length);
int ecsvm_text_buffer_append(ecsvm_text_buffer_t *buffer, const char *text);
int ecsvm_text_buffer_append_char(ecsvm_text_buffer_t *buffer, char ch);
int ecsvm_text_buffer_append_repeat(ecsvm_text_buffer_t *buffer, char ch, size_t count);
int ecsvm_text_buffer_append_indent(ecsvm_text_buffer_t *buffer, size_t indent);

#endif
