#ifndef ECSVM_SYSTEM_WINDOW_H
#define ECSVM_SYSTEM_WINDOW_H

#include "ecsvm/ecsvm.h"

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ecsvm_window_system ecsvm_window_system_t;

typedef struct ecsvm_window_config {
    const char *title;
    int width;
    int height;
    SDL_WindowFlags window_flags;
} ecsvm_window_config_t;

ecsvm_window_system_t *ecsvm_window_system_create(const ecsvm_window_config_t *config);
void ecsvm_window_system_destroy(ecsvm_window_system_t *system);

ecsvm_status_t ecsvm_window_system_register(
    ecsvm_engine_t *engine,
    ecsvm_window_system_t *system
);

int ecsvm_window_system_should_close(const ecsvm_window_system_t *system);
float ecsvm_window_system_delta_seconds(const ecsvm_window_system_t *system);
int ecsvm_window_system_size(const ecsvm_window_system_t *system, int *width, int *height);
SDL_Window *ecsvm_window_system_window(const ecsvm_window_system_t *system);
SDL_Renderer *ecsvm_window_system_renderer(const ecsvm_window_system_t *system);

#ifdef __cplusplus
}
#endif

#endif
