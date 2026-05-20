#include "ecsvm/system.h"

#include "system_window_internal.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct ecsvm_renderer_system_state {
    ecsvm_core_components_t components;
    ecsvm_vec4_t clear_color;
} ecsvm_renderer_system_state_t;

static ecsvm_renderer_system_state_t *ecsvm_renderer_system_state(ecsvm_engine_t *engine)
{
    ecsvm_system_t *system;

    if (engine == NULL) {
        return NULL;
    }

    system = engine->current_system(engine);
    return system != NULL
        ? (ecsvm_renderer_system_state_t *)system->get_userdata(system)
        : NULL;
}

static ecsvm_window_system_state_t *ecsvm_renderer_window_system(ecsvm_engine_t *engine)
{
    ecsvm_system_t *system;

    if (engine == NULL) {
        return NULL;
    }

    system = engine->get_system(engine, ECSVM_SYSTEM_WINDOW_NAME);
    return system != NULL
        ? (ecsvm_window_system_state_t *)system->get_userdata(system)
        : NULL;
}

static void renderer_log_error(ecsvm_engine_t *engine, const char *prefix)
{
    char message[512];

    if (engine == NULL) {
        return;
    }

    (void)snprintf(message, sizeof(message), "%s: %s", prefix, SDL_GetError());
    engine->log(engine, message);
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

static bool renderer_draw_circle(
    SDL_Renderer *renderer,
    const ecsvm_transform_component_t *transform,
    const ecsvm_graphics_shape_component_t *shape
)
{
    enum {
        ECSVM_CIRCLE_SEGMENTS = 24
    };
    SDL_Vertex vertices[ECSVM_CIRCLE_SEGMENTS + 2];
    int indices[ECSVM_CIRCLE_SEGMENTS * 3];
    const float pi = 3.14159265358979323846f;
    const float radius_x = transform->scale.x * 0.5f;
    const float radius_y = transform->scale.y * 0.5f;
    int index;

    vertices[0].position.x = transform->position.x;
    vertices[0].position.y = transform->position.y;
    vertices[0].color.r = shape->color.x;
    vertices[0].color.g = shape->color.y;
    vertices[0].color.b = shape->color.z;
    vertices[0].color.a = shape->color.w;
    vertices[0].tex_coord.x = 0.0f;
    vertices[0].tex_coord.y = 0.0f;

    for (index = 0; index <= ECSVM_CIRCLE_SEGMENTS; ++index) {
        const float angle = ((float)index / (float)ECSVM_CIRCLE_SEGMENTS) * pi * 2.0f;
        vertices[index + 1].position.x = transform->position.x + cosf(angle) * radius_x;
        vertices[index + 1].position.y = transform->position.y + sinf(angle) * radius_y;
        vertices[index + 1].color = vertices[0].color;
        vertices[index + 1].tex_coord.x = 0.0f;
        vertices[index + 1].tex_coord.y = 0.0f;
    }

    for (index = 0; index < ECSVM_CIRCLE_SEGMENTS; ++index) {
        indices[index * 3] = 0;
        indices[index * 3 + 1] = index + 1;
        indices[index * 3 + 2] = index + 2;
    }

    return SDL_RenderGeometry(
        renderer,
        NULL,
        vertices,
        ECSVM_CIRCLE_SEGMENTS + 2,
        indices,
        ECSVM_CIRCLE_SEGMENTS * 3
    );
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
    const ecsvm_renderer_system_state_t *config,
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

static ecsvm_status_t ecsvm_renderer_system_main(ecsvm_engine_t *engine)
{
    ecsvm_renderer_system_state_t *system;
    ecsvm_window_system_state_t *window_system;
    SDL_Renderer *renderer;
    size_t index;

    system = ecsvm_renderer_system_state(engine);
    window_system = ecsvm_renderer_window_system(engine);
    if (system == NULL || window_system == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    renderer = window_system->renderer;
    if (renderer == NULL) {
        return ECSVM_OK;
    }

    if (!SDL_SetRenderDrawColorFloat(
            renderer,
            system->clear_color.x,
            system->clear_color.y,
            system->clear_color.z,
            system->clear_color.w
        )) {
        renderer_log_error(engine, "SDL_SetRenderDrawColorFloat failed");
        return ECSVM_ERROR_CALLBACK;
    }

    if (!SDL_RenderClear(renderer)) {
        renderer_log_error(engine, "SDL_RenderClear failed");
        return ECSVM_ERROR_CALLBACK;
    }

    for (index = 0u; index < ecsvm_entity_count(engine); ++index) {
        ecsvm_entity_t entity;
        ecsvm_transform_component_t transform;
        const ecsvm_graphics_shape_component_t *shape;

        entity = ecsvm_entity_at(engine, index);
        shape = (const ecsvm_graphics_shape_component_t *)ecsvm_component_get(
            engine,
            system->components.graphics_shape,
            entity
        );
        if (shape == NULL ||
            !renderer_resolve_transform(
                engine,
                system,
                entity,
                ecsvm_entity_count(engine),
                &transform
            )) {
            continue;
        }

        if (shape->kind == ECSVM_SHAPE_RECTANGLE) {
            if (!renderer_draw_rectangle(renderer, &transform, shape)) {
                renderer_log_error(engine, "SDL_RenderGeometry failed");
                return ECSVM_ERROR_CALLBACK;
            }
        } else if (shape->kind == ECSVM_SHAPE_CIRCLE) {
            if (!renderer_draw_circle(renderer, &transform, shape)) {
                renderer_log_error(engine, "SDL_RenderGeometry failed");
                return ECSVM_ERROR_CALLBACK;
            }
        }
    }

    if (!SDL_RenderPresent(renderer)) {
        renderer_log_error(engine, "SDL_RenderPresent failed");
        return ECSVM_ERROR_CALLBACK;
    }

    return ECSVM_OK;
}

static void ecsvm_renderer_system_dispose(ecsvm_engine_t *engine, ecsvm_system_t *system)
{
    ecsvm_renderer_system_state_t *state;

    if (engine == NULL || system == NULL) {
        return;
    }

    state = (ecsvm_renderer_system_state_t *)system->get_userdata(system);
    if (state != NULL) {
        engine->free(engine, state);
    }
}

ecsvm_status_t ecsvm_system_renderer_register(ecsvm_engine_t *engine)
{
    static const char *const after_names[] = {
        ECSVM_SYSTEM_WINDOW_NAME
    };
    ecsvm_system_definition_t definition;
    ecsvm_renderer_system_state_t *system;
    ecsvm_status_t status;

    if (engine == NULL || engine->get_system(engine, ECSVM_SYSTEM_WINDOW_NAME) == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    system = (ecsvm_renderer_system_state_t *)engine->alloc(engine, sizeof(*system));
    if (system == NULL) {
        return ECSVM_ERROR_MEMORY;
    }

    memset(system, 0, sizeof(*system));
    system->components.hierarchy = ecsvm_engine_hierarchy_component(engine);
    system->components.transform = ecsvm_engine_find_component(engine, "core.Transform");
    system->components.time = ecsvm_engine_find_component(engine, "core.Time");
    system->components.graphics_shape = ecsvm_engine_find_component(engine, "core.graphics.GraphicsShape");
    if (system->components.transform == ECSVM_INVALID_COMPONENT ||
        system->components.graphics_shape == ECSVM_INVALID_COMPONENT) {
        engine->free(engine, system);
        return ECSVM_ERROR_ARGUMENT;
    }

    system->clear_color.x = 0.05f;
    system->clear_color.y = 0.05f;
    system->clear_color.z = 0.08f;
    system->clear_color.w = 1.0f;

    memset(&definition, 0, sizeof(definition));
    definition.name = ECSVM_SYSTEM_RENDERER_NAME;
    definition.main = ecsvm_renderer_system_main;
    definition.dispose = ecsvm_renderer_system_dispose;
    definition.userdata = system;
    definition.after = after_names;
    definition.after_count = sizeof(after_names) / sizeof(after_names[0]);
    status = engine->register_system(engine, &definition);
    if (status != ECSVM_OK) {
        engine->free(engine, system);
        return status;
    }

    return ECSVM_OK;
}
