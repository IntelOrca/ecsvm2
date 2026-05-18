#include "ecsvm/system_window.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>

struct ecsvm_window_system {
    char *title;
    int width;
    int height;
    uint64_t window_flags;
    ecsvm_native_window_t *window;
    ecsvm_native_renderer_t *renderer;
    Uint64 previous_counter;
    float delta_seconds;
    bool initialized;
    bool should_close;
};

static char *window_copy_string(const char *text)
{
    char *copy;
    size_t length;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1u);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1u);
    return copy;
}

static void window_log_error(ecsvm_context_t *ctx, const char *prefix)
{
    char message[512];

    (void)snprintf(message, sizeof(message), "%s: %s", prefix, SDL_GetError());
    ctx->api.log(ctx->api.userdata, message);
}

static bool window_initialize(ecsvm_context_t *ctx, ecsvm_window_system_t *system)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        window_log_error(ctx, "SDL_Init failed");
        return false;
    }

    if (!SDL_CreateWindowAndRenderer(
            system->title,
            system->width,
            system->height,
            (SDL_WindowFlags)system->window_flags,
            &system->window,
            &system->renderer
        )) {
        window_log_error(ctx, "SDL_CreateWindowAndRenderer failed");
        SDL_Quit();
        return false;
    }

    system->previous_counter = SDL_GetPerformanceCounter();
    system->delta_seconds = 1.0f / 60.0f;
    system->initialized = true;
    return true;
}

static ecsvm_status_t ecsvm_window_system_tick(ecsvm_context_t *ctx)
{
    ecsvm_window_system_t *system;
    SDL_Event event;
    Uint64 current_counter;
    Uint64 frequency;
    int keyboard_count;
    int escape_index;
    const bool *keyboard_state;

    system = (ecsvm_window_system_t *)ctx->api.userdata;
    if (system == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    if (!system->initialized && !window_initialize(ctx, system)) {
        return ECSVM_ERROR_CALLBACK;
    }

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            system->should_close = true;
        }
    }

    keyboard_state = SDL_GetKeyboardState(&keyboard_count);
    escape_index = (int)SDL_SCANCODE_ESCAPE;
    if (keyboard_state != NULL &&
        escape_index >= 0 &&
        escape_index < keyboard_count &&
        keyboard_state[escape_index]) {
        system->should_close = true;
    }

    current_counter = SDL_GetPerformanceCounter();
    frequency = SDL_GetPerformanceFrequency();
    if (system->previous_counter != 0u && frequency != 0u) {
        system->delta_seconds = (float)(current_counter - system->previous_counter) /
            (float)frequency;
    } else {
        system->delta_seconds = 1.0f / 60.0f;
    }

    system->previous_counter = current_counter;
    if (!SDL_GetWindowSize(system->window, &system->width, &system->height)) {
        window_log_error(ctx, "SDL_GetWindowSize failed");
        return ECSVM_ERROR_CALLBACK;
    }

    return ECSVM_OK;
}

ecsvm_window_system_t *ecsvm_window_system_create(const ecsvm_window_config_t *config)
{
    ecsvm_window_system_t *system;

    if (config == NULL || config->title == NULL || config->width <= 0 || config->height <= 0) {
        return NULL;
    }

    system = (ecsvm_window_system_t *)calloc(1u, sizeof(*system));
    if (system == NULL) {
        return NULL;
    }

    system->title = window_copy_string(config->title);
    if (system->title == NULL) {
        free(system);
        return NULL;
    }

    system->width = config->width;
    system->height = config->height;
    system->window_flags = config->window_flags;
    system->delta_seconds = 1.0f / 60.0f;
    return system;
}

void ecsvm_window_system_destroy(ecsvm_window_system_t *system)
{
    if (system == NULL) {
        return;
    }

    if (system->renderer != NULL) {
        SDL_DestroyRenderer(system->renderer);
    }

    if (system->window != NULL) {
        SDL_DestroyWindow(system->window);
    }

    if (system->initialized) {
        SDL_Quit();
    }

    free(system->title);
    free(system);
}

ecsvm_status_t ecsvm_window_system_register(
    ecsvm_engine_t *engine,
    ecsvm_window_system_t *system
)
{
    ecsvm_system_desc_t desc;

    if (engine == NULL || system == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    memset(&desc, 0, sizeof(desc));
    desc.name = "core.Window";
    desc.callback = ecsvm_window_system_tick;
    desc.user_data = system;
    return ecsvm_engine_register_system(engine, &desc, NULL);
}

int ecsvm_window_system_should_close(const ecsvm_window_system_t *system)
{
    if (system == NULL) {
        return 1;
    }

    return system->should_close ? 1 : 0;
}

float ecsvm_window_system_delta_seconds(const ecsvm_window_system_t *system)
{
    if (system == NULL) {
        return 1.0f / 60.0f;
    }

    return system->delta_seconds;
}

int ecsvm_window_system_size(const ecsvm_window_system_t *system, int *width, int *height)
{
    if (system == NULL) {
        return 0;
    }

    if (width != NULL) {
        *width = system->width;
    }

    if (height != NULL) {
        *height = system->height;
    }

    return 1;
}

ecsvm_native_window_t *ecsvm_window_system_window(const ecsvm_window_system_t *system)
{
    if (system == NULL) {
        return NULL;
    }

    return system->window;
}

ecsvm_native_renderer_t *ecsvm_window_system_renderer(const ecsvm_window_system_t *system)
{
    if (system == NULL) {
        return NULL;
    }

    return system->renderer;
}
