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

static void renderer_compose_transform(
    const ecsvm_transform_component_t *parent,
    const ecsvm_transform_component_t *local,
    ecsvm_transform_component_t *out_transform
)
{
    const float sine = sinf(parent->rotation);
    const float cosine = cosf(parent->rotation);
    const float scaled_x = local->position.x * parent->scale.x;
    const float scaled_y = local->position.y * parent->scale.y;

    out_transform->position.x = parent->position.x + (scaled_x * cosine) - (scaled_y * sine);
    out_transform->position.y = parent->position.y + (scaled_x * sine) + (scaled_y * cosine);
    out_transform->scale.x = parent->scale.x * local->scale.x;
    out_transform->scale.y = parent->scale.y * local->scale.y;
    out_transform->rotation = parent->rotation + local->rotation;
}

static bool renderer_resolve_transform(
    const ecsvm_engine_t *engine,
    const ecsvm_renderer_config_t *config,
    ecsvm_entity_t entity,
    size_t depth_limit,
    ecsvm_transform_component_t *out_transform
)
{
    const ecsvm_transform_component_t *local_transform;
    const ecsvm_hierarchy_component_t *hierarchy;

    local_transform = (const ecsvm_transform_component_t *)ecsvm_component_get(
        engine,
        config->components.transform,
        entity
    );
    if (local_transform == NULL || out_transform == NULL) {
        return false;
    }

    *out_transform = *local_transform;
    if (config->components.hierarchy == ECSVM_INVALID_COMPONENT || depth_limit == 0u) {
        return true;
    }

    hierarchy = (const ecsvm_hierarchy_component_t *)ecsvm_component_get(
        engine,
        config->components.hierarchy,
        entity
    );
    if (hierarchy == NULL || hierarchy->parent == ECSVM_INVALID_ENTITY) {
        return true;
    }

    {
        ecsvm_transform_component_t parent_transform;

        if (!renderer_resolve_transform(
                engine,
                config,
                hierarchy->parent,
                depth_limit - 1u,
                &parent_transform
            )) {
            return true;
        }

        renderer_compose_transform(&parent_transform, local_transform, out_transform);
    }

    return true;
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
        ecsvm_transform_component_t transform;
        const ecsvm_graphics_shape_component_t *shape;

        entity = ecsvm_entity_at(ctx->engine, index);
        shape = (const ecsvm_graphics_shape_component_t *)ecsvm_component_get(
            ctx->engine,
            system->config.components.graphics_shape,
            entity
        );
        if (shape == NULL ||
            !renderer_resolve_transform(
                ctx->engine,
                &system->config,
                entity,
                ecsvm_entity_count(ctx->engine),
                &transform
            )) {
            continue;
        }

        if (shape->kind == ECSVM_SHAPE_RECTANGLE &&
            !renderer_draw_rectangle(renderer, &transform, shape)) {
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
    desc.name = "core.Renderer";
    desc.callback = ecsvm_renderer_system_tick;
    desc.user_data = system;
    return ecsvm_engine_register_system(engine, &desc, NULL);
}
