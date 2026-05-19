#include "ecsvm/system_window.h"
#include "ecsvm/component.h"

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
    ecsvm_component_id_t window_component;
    ecsvm_component_id_t input_monitor_component;
    ecsvm_entity_t window_entity;
    bool initialized;
    bool close_pending;
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

static ecsvm_entity_t window_find_component_entity(
    const ecsvm_engine_t *engine,
    ecsvm_component_id_t component_id
)
{
    size_t index;

    if (engine == NULL || component_id == ECSVM_INVALID_COMPONENT) {
        return ECSVM_INVALID_ENTITY;
    }

    for (index = 0u; index < ecsvm_entity_count(engine); ++index) {
        ecsvm_entity_t entity;

        entity = ecsvm_entity_at(engine, index);
        if (entity != ECSVM_INVALID_ENTITY &&
            ecsvm_component_has(engine, component_id, entity)) {
            return entity;
        }
    }

    return ECSVM_INVALID_ENTITY;
}

static ecsvm_window_component_t *window_ensure_component(
    ecsvm_context_t *ctx,
    ecsvm_window_system_t *system
)
{
    ecsvm_window_component_t initial;

    if (system->window_component == ECSVM_INVALID_COMPONENT) {
        system->window_component = ecsvm_engine_find_component(ctx->engine, "core.ui.Window");
    }
    if (system->window_component == ECSVM_INVALID_COMPONENT) {
        return NULL;
    }

    if (system->window_entity == ECSVM_INVALID_ENTITY ||
        !ecsvm_component_has(ctx->engine, system->window_component, system->window_entity)) {
        system->window_entity = window_find_component_entity(ctx->engine, system->window_component);
    }

    if (system->window_entity == ECSVM_INVALID_ENTITY) {
        memset(&initial, 0, sizeof(initial));
        initial.width = system->width;
        initial.height = system->height;
        initial.delta_seconds = system->delta_seconds;
        initial.closing = 0u;

        system->window_entity = ecsvm_entity_create(ctx->engine);
        if (system->window_entity == ECSVM_INVALID_ENTITY ||
            ecsvm_component_set(
                ctx->engine,
                system->window_component,
                system->window_entity,
                &initial
            ) != ECSVM_OK) {
            system->window_entity = ECSVM_INVALID_ENTITY;
            return NULL;
        }
    }

    return (ecsvm_window_component_t *)ecsvm_component_get_mutable(
        ctx->engine,
        system->window_component,
        system->window_entity
    );
}

static float window_clamp_float(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static float window_monitor_value(
    const ecsvm_input_monitor_component_t *monitor,
    const bool *keyboard_state,
    int keyboard_count,
    float mouse_x,
    float mouse_y,
    SDL_MouseButtonFlags mouse_buttons,
    int mouse_in_window,
    int window_width,
    int window_height,
    float previous_value
)
{
    if (monitor == NULL) {
        return 0.0f;
    }

    if (monitor->device == ECSVM_INPUT_DEVICE_KEYBOARD &&
        monitor->kind == ECSVM_INPUT_MONITOR_BUTTON &&
        keyboard_state != NULL &&
        monitor->index >= 0 &&
        monitor->index < keyboard_count) {
        return keyboard_state[monitor->index] ? 1.0f : 0.0f;
    }

    if (monitor->device == ECSVM_INPUT_DEVICE_MOUSE) {
        if (monitor->kind == ECSVM_INPUT_MONITOR_AXIS) {
            if (!mouse_in_window) {
                return previous_value;
            }
            if (monitor->index == ECSVM_INPUT_MOUSE_X) {
                return window_clamp_float(mouse_x, 0.0f, (float)window_width);
            }
            if (monitor->index == ECSVM_INPUT_MOUSE_Y) {
                return window_clamp_float(mouse_y, 0.0f, (float)window_height);
            }
        }

        if (monitor->kind == ECSVM_INPUT_MONITOR_BUTTON) {
            if (monitor->index <= 0) {
                return 0.0f;
            }
            return (mouse_buttons & SDL_BUTTON_MASK((Uint32)monitor->index)) != 0u ? 1.0f : 0.0f;
        }
    }

    return 0.0f;
}

static ecsvm_status_t window_update_input_monitors(ecsvm_context_t *ctx, ecsvm_window_system_t *system)
{
    size_t index;
    int keyboard_count;
    int mouse_in_window;
    float mouse_x;
    float mouse_y;
    SDL_MouseButtonFlags mouse_buttons;
    const bool *keyboard_state;

    if (system->input_monitor_component == ECSVM_INVALID_COMPONENT) {
        system->input_monitor_component = ecsvm_engine_find_component(
            ctx->engine,
            "core.input.InputMonitor"
        );
    }
    if (system->input_monitor_component == ECSVM_INVALID_COMPONENT) {
        return ECSVM_OK;
    }

    keyboard_state = SDL_GetKeyboardState(&keyboard_count);
    mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
    mouse_in_window = SDL_GetMouseFocus() == system->window;
    for (index = 0u; index < ecsvm_entity_count(ctx->engine); ++index) {
        ecsvm_entity_t entity;
        ecsvm_input_monitor_component_t *monitor;

        entity = ecsvm_entity_at(ctx->engine, index);
        if (entity == ECSVM_INVALID_ENTITY ||
            !ecsvm_component_has(ctx->engine, system->input_monitor_component, entity)) {
            continue;
        }

        monitor = (ecsvm_input_monitor_component_t *)ecsvm_component_get_mutable(
            ctx->engine,
            system->input_monitor_component,
            entity
        );
        if (monitor == NULL) {
            return ECSVM_ERROR_NOT_FOUND;
        }

        monitor->value = window_monitor_value(
            monitor,
            keyboard_state,
            keyboard_count,
            mouse_x,
            mouse_y,
            mouse_buttons,
            mouse_in_window,
            system->width,
            system->height,
            monitor->value
        );
    }

    return ECSVM_OK;
}

static ecsvm_status_t ecsvm_window_system_tick(ecsvm_context_t *ctx)
{
    ecsvm_window_system_t *system;
    ecsvm_window_component_t *window_component;
    SDL_Event event;
    Uint64 current_counter;
    Uint64 frequency;
    bool close_requested;
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

    close_requested = false;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            close_requested = true;
        }
    }

    keyboard_state = SDL_GetKeyboardState(&keyboard_count);
    escape_index = (int)SDL_SCANCODE_ESCAPE;
    if (keyboard_state != NULL &&
        escape_index >= 0 &&
        escape_index < keyboard_count &&
        keyboard_state[escape_index]) {
        close_requested = true;
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

    window_component = window_ensure_component(ctx, system);
    if (system->window_component != ECSVM_INVALID_COMPONENT && window_component == NULL) {
        return ECSVM_ERROR_CALLBACK;
    }

    if (window_component != NULL) {
        if (system->close_pending && window_component->closing) {
            system->should_close = true;
            ecsvm_engine_request_stop(ctx->engine);
        }

        window_component->width = system->width;
        window_component->height = system->height;
        window_component->delta_seconds = system->delta_seconds;
        if (close_requested) {
            window_component->closing = 1u;
        }

        system->close_pending = window_component->closing != 0u;
    } else if (close_requested) {
        system->should_close = true;
        system->close_pending = true;
        ecsvm_engine_request_stop(ctx->engine);
    } else {
        system->close_pending = false;
    }

    if (window_update_input_monitors(ctx, system) != ECSVM_OK) {
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
    system->window_component = ECSVM_INVALID_COMPONENT;
    system->input_monitor_component = ECSVM_INVALID_COMPONENT;
    system->window_entity = ECSVM_INVALID_ENTITY;
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
