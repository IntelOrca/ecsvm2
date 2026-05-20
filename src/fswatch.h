#ifndef ECSVM_FSWATCH_H
#define ECSVM_FSWATCH_H

#include <stddef.h>
#include <stdint.h>

typedef struct ecsvm_fswatch_entry {
    char *path;
    uint64_t modified_time;
} ecsvm_fswatch_entry_t;

typedef struct ecsvm_fswatch {
    char *root_path;
    ecsvm_fswatch_entry_t *entries;
    size_t count;
    size_t capacity;
} ecsvm_fswatch_t;

int ecsvm_fswatch_init(
    ecsvm_fswatch_t *watch,
    const char *root_path,
    char *error_message,
    size_t error_message_capacity
);

void ecsvm_fswatch_free(ecsvm_fswatch_t *watch);

int ecsvm_fswatch_poll(
    ecsvm_fswatch_t *watch,
    int *out_changed,
    char *error_message,
    size_t error_message_capacity
);

#endif
