#ifndef ECSVM_SYSTEM_H
#define ECSVM_SYSTEM_H

#include "ecsvm/component.h"
#include "ecsvm/ecsvm.h"

#include <stdint.h>

#if ECSVM_ENABLE_SDL3
#include <SDL3/SDL.h>
typedef SDL_Window ecsvm_native_window_t;
typedef SDL_Renderer ecsvm_native_renderer_t;
#else
typedef struct ecsvm_native_window ecsvm_native_window_t;
typedef struct ecsvm_native_renderer ecsvm_native_renderer_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ECSVM_SYSTEM_TIME_NAME "core.Time"
#define ECSVM_SYSTEM_WINDOW_NAME "core.Window"
#define ECSVM_SYSTEM_RENDERER_NAME "core.Renderer"

ecsvm_status_t ecsvm_system_time_register(ecsvm_engine_t *engine);
ecsvm_status_t ecsvm_system_window_register(ecsvm_engine_t *engine);
ecsvm_status_t ecsvm_system_renderer_register(ecsvm_engine_t *engine);

#ifdef __cplusplus
}
#endif

#endif
