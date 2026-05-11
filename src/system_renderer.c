#include "ecsvm/system_renderer.h"

#include <SDL3/SDL.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ecsvm_renderer_system {
    ecsvm_window_system_t *window_system;
    ecsvm_renderer_config_t config;
};

static void renderer_log_error(ecsvm_context_t *ctx, const char *prefix)
{
    char message[512];

    (void)snprintf(message, sizeof(message), "%s: %s", prefix, SDL_GetError());
    ctx->api.log(ctx->api.userdata, message);
}

static bool renderer_draw_rectangle(
    SDL_Renderer *renderer,
    const ecsvm_transform_component_t *transform,
    const ecsvm_graphics_shape_component_t *shape
)
{
    SDL_Vertex vertices[4];
    const int indices[6] = {0, 1, 2, 0, 2, 3};
    const float half_width = transform->scale.x * 0.5f;
    const float half_height = transform->scale.y * 0.5f;
    const float sine = sinf(transform->rotation);
    const float cosine = cosf(transform->rotation);
    const ecsvm_vec2_t corners[4] = {
        {-half_width, -half_height},
        {half_width, -half_height},
        {half_width, half_height},
        {-half_width, half_height}
    };
    int index;

    for (index = 0; index < 4; ++index) {
        const float local_x = corners[index].x;
        const float local_y = corners[index].y;
        vertices[index].position.x = transform->position.x + (local_x * cosine) - (local_y * sine);
        vertices[index].position.y = transform->position.y + (local_x * sine) + (local_y * cosine);
        vertices[index].color.r = shape->color.x;
        vertices[index].color.g = shape->color.y;
        vertices[index].color.b = shape->color.z;
        vertices[index].color.a = shape->color.w;
        vertices[index].tex_coord.x = 0.0f;
        vertices[index].tex_coord.y = 0.0f;
    }

    return SDL_RenderGeometry(renderer, NULL, vertices, 4, indices, 6);
}

static ecsvm_status_t ecsvm_renderer_system_tick(ecsvm_context_t *ctx)
{
    ecsvm_renderer_system_t *system;
    SDL_Renderer *renderer;
    size_t index;

    system = (ecsvm_renderer_system_t *)ctx->api.userdata;
    if (system == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    renderer = ecsvm_window_system_renderer(system->window_system);
    if (renderer == NULL) {
        return ECSVM_OK;
    }

    if (!SDL_SetRenderDrawColorFloat(
            renderer,
            system->config.clear_color.x,
            system->config.clear_color.y,
            system->config.clear_color.z,
            system->config.clear_color.w
        )) {
        renderer_log_error(ctx, "SDL_SetRenderDrawColorFloat failed");
        return ECSVM_ERROR_CALLBACK;
    }

    if (!SDL_RenderClear(renderer)) {
        renderer_log_error(ctx, "SDL_RenderClear failed");
        return ECSVM_ERROR_CALLBACK;
    }

    for (index = 0u; index < ecsvm_entity_count(ctx->engine); ++index) {
        ecsvm_entity_t entity;
        const ecsvm_transform_component_t *transform;
        const ecsvm_graphics_shape_component_t *shape;

        entity = ecsvm_entity_at(ctx->engine, index);
        transform = (const ecsvm_transform_component_t *)ecsvm_component_get(
            ctx->engine,
            system->config.components.transform,
            entity
        );
        shape = (const ecsvm_graphics_shape_component_t *)ecsvm_component_get(
            ctx->engine,
            system->config.components.graphics_shape,
            entity
        );
        if (transform == NULL || shape == NULL) {
            continue;
        }

        if (shape->kind == ECSVM_SHAPE_RECTANGLE &&
            !renderer_draw_rectangle(renderer, transform, shape)) {
            renderer_log_error(ctx, "SDL_RenderGeometry failed");
            return ECSVM_ERROR_CALLBACK;
        }
    }

    if (!SDL_RenderPresent(renderer)) {
        renderer_log_error(ctx, "SDL_RenderPresent failed");
        return ECSVM_ERROR_CALLBACK;
    }

    return ECSVM_OK;
}

ecsvm_renderer_system_t *ecsvm_renderer_system_create(
    ecsvm_window_system_t *window_system,
    const ecsvm_renderer_config_t *config
)
{
    ecsvm_renderer_system_t *system;

    if (window_system == NULL || config == NULL) {
        return NULL;
    }

    system = (ecsvm_renderer_system_t *)calloc(1u, sizeof(*system));
    if (system == NULL) {
        return NULL;
    }

    system->window_system = window_system;
    system->config = *config;
    return system;
}

void ecsvm_renderer_system_destroy(ecsvm_renderer_system_t *system)
{
    free(system);
}

ecsvm_status_t ecsvm_renderer_system_register(
    ecsvm_engine_t *engine,
    ecsvm_renderer_system_t *system
)
{
    ecsvm_system_desc_t desc;

    if (engine == NULL || system == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    memset(&desc, 0, sizeof(desc));
    desc.name = "core.renderer";
    desc.callback = ecsvm_renderer_system_tick;
    desc.user_data = system;
    return ecsvm_engine_register_system(engine, &desc, NULL);
}
