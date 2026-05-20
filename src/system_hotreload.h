#ifndef ECSVM_SYSTEM_HOTRELOAD_H
#define ECSVM_SYSTEM_HOTRELOAD_H

#include "ecsvm/ecsvm.h"

#include "fswatch.h"

typedef struct ecsvm_hotreload_system {
    ecsvm_fswatch_t watcher;
    int reload_requested;
} ecsvm_hotreload_system_t;

int ecsvm_hotreload_system_init(
    ecsvm_hotreload_system_t *system,
    const char *project_path,
    char *error_message,
    size_t error_message_capacity
);

void ecsvm_hotreload_system_free(ecsvm_hotreload_system_t *system);

int ecsvm_hotreload_system_consume_reload(ecsvm_hotreload_system_t *system);

ecsvm_status_t ecsvm_hotreload_system_register(
    ecsvm_engine_t *engine,
    ecsvm_hotreload_system_t *system
);

#endif
