#ifndef ECSVM_SYSTEM_WINDOW_INTERNAL_H
#define ECSVM_SYSTEM_WINDOW_INTERNAL_H

#include "ecsvm/system.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct ecsvm_window_system_state {
    char *title;
    int width;
    int height;
    uint64_t window_flags;
    ecsvm_native_window_t *window;
    ecsvm_native_renderer_t *renderer;
    Uint64 previous_counter;
    float delta_seconds;
    ecsvm_component_id_t window_component;
    ecsvm_component_id_t input_monitor_component;
    ecsvm_entity_t window_entity;
    bool initialized;
    bool close_pending;
    bool should_close;
} ecsvm_window_system_state_t;

#endif
