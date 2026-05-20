#include "text_buffer.h"

#include "utility.h"

#include <stdlib.h>
#include <string.h>

int ecsvm_text_buffer_reserve(ecsvm_text_buffer_t *buffer, size_t additional)
{
    size_t required;
    size_t capacity;
    char *data;

    if (buffer == NULL) {
        return 0;
    }

    required = buffer->length + additional + 1u;
    if (required <= buffer->capacity) {
        return 1;
    }

    capacity = buffer->capacity == 0u ? 128u : buffer->capacity;
    while (capacity < required) {
        capacity = ecsvm_next_capacity(capacity, required);
    }

    data = (char *)realloc(buffer->data, capacity);
    if (data == NULL) {
        return 0;
    }

    buffer->data = data;
    buffer->capacity = capacity;
    return 1;
}

int ecsvm_text_buffer_append_range(ecsvm_text_buffer_t *buffer, const char *text, size_t length)
{
    if (!ecsvm_text_buffer_reserve(buffer, length)) {
        return 0;
    }

    if (length > 0u) {
        memcpy(buffer->data + buffer->length, text, length);
        buffer->length += length;
    }
    buffer->data[buffer->length] = '\0';
    return 1;
}

int ecsvm_text_buffer_append(ecsvm_text_buffer_t *buffer, const char *text)
{
    return text != NULL && ecsvm_text_buffer_append_range(buffer, text, strlen(text));
}

int ecsvm_text_buffer_append_char(ecsvm_text_buffer_t *buffer, char ch)
{
    if (!ecsvm_text_buffer_reserve(buffer, 1u)) {
        return 0;
    }

    buffer->data[buffer->length] = ch;
    buffer->length += 1u;
    buffer->data[buffer->length] = '\0';
    return 1;
}

int ecsvm_text_buffer_append_repeat(ecsvm_text_buffer_t *buffer, char ch, size_t count)
{
    size_t index;

    for (index = 0u; index < count; ++index) {
        if (!ecsvm_text_buffer_append_char(buffer, ch)) {
            return 0;
        }
    }
    return 1;
}

int ecsvm_text_buffer_append_indent(ecsvm_text_buffer_t *buffer, size_t indent)
{
    size_t index;

    for (index = 0u; index < indent; ++index) {
        if (!ecsvm_text_buffer_append(buffer, "    ")) {
            return 0;
        }
    }
    return 1;
}
