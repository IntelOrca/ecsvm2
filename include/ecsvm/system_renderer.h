#ifndef ECSVM_SYSTEM_RENDERER_H
#define ECSVM_SYSTEM_RENDERER_H

#include "ecsvm/component.h"
#include "ecsvm/ecsvm.h"
#include "ecsvm/system_window.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ecsvm_renderer_system ecsvm_renderer_system_t;

typedef struct ecsvm_renderer_config {
    ecsvm_core_components_t components;
    ecsvm_vec4_t clear_color;
} ecsvm_renderer_config_t;

ecsvm_renderer_system_t *ecsvm_renderer_system_create(
    ecsvm_window_system_t *window_system,
    const ecsvm_renderer_config_t *config
);

void ecsvm_renderer_system_destroy(ecsvm_renderer_system_t *system);

ecsvm_status_t ecsvm_renderer_system_register(
    ecsvm_engine_t *engine,
    ecsvm_renderer_system_t *system
);

#ifdef __cplusplus
}
#endif

#endif
