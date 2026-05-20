#ifndef ECSVM_FILE_UTIL_H
#define ECSVM_FILE_UTIL_H

#include <stddef.h>

int ecsvm_read_text_file(const char *path, char **out_text, size_t *out_length);

#endif
