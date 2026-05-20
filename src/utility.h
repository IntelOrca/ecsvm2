#ifndef ECSVM_UTILITY_H
#define ECSVM_UTILITY_H

#include <stddef.h>
#include <stdio.h>

#ifdef _MSC_VER
#define ECSVM_ALIGNOF(type) __alignof(type)
#else
#define ECSVM_ALIGNOF(type) offsetof(struct { char pad; type value; }, value)
#endif

#ifdef _WIN32
typedef __int64 ecsvm_file_offset_t;
#define ECSVM_FTELL _ftelli64
#define ECSVM_FSEEK _fseeki64
#else
typedef long ecsvm_file_offset_t;
#define ECSVM_FTELL ftell
#define ECSVM_FSEEK fseek
#endif

void ecsvm_set_error(char *error_message, size_t capacity, const char *message);
char *ecsvm_copy_string(const char *text);
char *ecsvm_copy_string_range(const char *text, size_t length);
size_t ecsvm_next_capacity(size_t current, size_t minimum);
int ecsvm_reserve_bytes(void **items, size_t item_size, size_t *capacity, size_t minimum);

#endif
