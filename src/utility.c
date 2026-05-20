#include "utility.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ecsvm_set_error(char *error_message, size_t capacity, const char *message)
{
    if (error_message == NULL || capacity == 0u) {
        return;
    }

    if (message == NULL) {
        error_message[0] = '\0';
        return;
    }

    (void)snprintf(error_message, capacity, "%s", message);
}

char *ecsvm_copy_string(const char *text)
{
    size_t length;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    return ecsvm_copy_string_range(text, length);
}

char *ecsvm_copy_string_range(const char *text, size_t length)
{
    char *copy;

    copy = (char *)malloc(length + 1u);
    if (copy == NULL) {
        return NULL;
    }

    if (length > 0u) {
        memcpy(copy, text, length);
    }
    copy[length] = '\0';
    return copy;
}

size_t ecsvm_next_capacity(size_t current, size_t minimum)
{
    size_t capacity;

    capacity = current == 0u ? 4u : current;
    while (capacity < minimum) {
        capacity *= 2u;
    }

    return capacity;
}

int ecsvm_reserve_bytes(void **items, size_t item_size, size_t *capacity, size_t minimum)
{
    void *memory;
    size_t new_capacity;

    if (minimum <= *capacity) {
        return 1;
    }

    new_capacity = ecsvm_next_capacity(*capacity, minimum);
    memory = realloc(*items, item_size * new_capacity);
    if (memory == NULL) {
        return 0;
    }

    *items = memory;
    *capacity = new_capacity;
    return 1;
}
