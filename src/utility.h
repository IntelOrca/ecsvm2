#ifndef ECSVM_UTILITY_H
#define ECSVM_UTILITY_H

#include <stddef.h>

void ecsvm_set_error(char *error_message, size_t capacity, const char *message);
char *ecsvm_copy_string(const char *text);
char *ecsvm_copy_string_range(const char *text, size_t length);
size_t ecsvm_next_capacity(size_t current, size_t minimum);
int ecsvm_reserve_bytes(void **items, size_t item_size, size_t *capacity, size_t minimum);

#endif
