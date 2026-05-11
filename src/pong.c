#include "ecsvm/component.h"
#include "ecsvm/ecsvm.h"
#include "ecsvm/system_renderer.h"
#include "ecsvm/system_window.h"

#include <SDL3/SDL.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct pong_velocity_component {
    ecsvm_vec2_t value;
} pong_velocity_component_t;

typedef struct pong_paddle_component {
    float speed;
    SDL_Scancode up_key;
    SDL_Scancode down_key;
} pong_paddle_component_t;

typedef struct pong_state {
    ecsvm_core_components_t core;
    ecsvm_component_id_t paddle_component;
    ecsvm_component_id_t velocity_component;
    ecsvm_window_system_t *window_system;
    ecsvm_entity_t left_paddle;
    ecsvm_entity_t right_paddle;
    ecsvm_entity_t ball;
    float ball_speed;
} pong_state_t;

static float pong_clamp(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }

    if (value > maximum) {
        return maximum;
    }

    return value;
}

static int pong_attach_transform(
    ecsvm_engine_t *engine,
    ecsvm_component_id_t transform_component,
    ecsvm_entity_t entity,
    float x,
    float y,
    float width,
    float height,
    float rotation
)
{
    ecsvm_transform_component_t transform;

    transform.position.x = x;
    transform.position.y = y;
    transform.scale.x = width;
    transform.scale.y = height;
    transform.rotation = rotation;
    return ecsvm_component_set(engine, transform_component, entity, &transform) == ECSVM_OK;
}

static int pong_attach_shape(
    ecsvm_engine_t *engine,
    ecsvm_component_id_t shape_component,
    ecsvm_entity_t entity,
    float r,
    float g,
    float b,
    float a
)
{
    ecsvm_graphics_shape_component_t shape;

    shape.color.x = r;
    shape.color.y = g;
    shape.color.z = b;
    shape.color.w = a;
    shape.kind = ECSVM_SHAPE_RECTANGLE;
    return ecsvm_component_set(engine, shape_component, entity, &shape) == ECSVM_OK;
}

static int pong_attach_paddle(
    ecsvm_engine_t *engine,
    ecsvm_component_id_t paddle_component,
    ecsvm_entity_t entity,
    float speed,
    SDL_Scancode up_key,
    SDL_Scancode down_key
)
{
    pong_paddle_component_t paddle;

    paddle.speed = speed;
    paddle.up_key = up_key;
    paddle.down_key = down_key;
    return ecsvm_component_set(engine, paddle_component, entity, &paddle) == ECSVM_OK;
}

static int pong_attach_velocity(
    ecsvm_engine_t *engine,
    ecsvm_component_id_t velocity_component,
    ecsvm_entity_t entity,
    float x,
    float y
)
{
    pong_velocity_component_t velocity;

    velocity.value.x = x;
    velocity.value.y = y;
    return ecsvm_component_set(engine, velocity_component, entity, &velocity) == ECSVM_OK;
}

static void pong_reset_ball(
    ecsvm_engine_t *engine,
    const pong_state_t *state,
    int width,
    int height,
    float direction
)
{
    ecsvm_transform_component_t *transform;
    pong_velocity_component_t *velocity;

    transform = (ecsvm_transform_component_t *)ecsvm_component_get_mutable(
        engine,
        state->core.transform,
        state->ball
    );
    velocity = (pong_velocity_component_t *)ecsvm_component_get_mutable(
        engine,
        state->velocity_component,
        state->ball
    );
    if (transform == NULL || velocity == NULL) {
        return;
    }

    transform->position.x = (float)width * 0.5f;
    transform->position.y = (float)height * 0.5f;
    transform->rotation = 0.0f;
    velocity->value.x = state->ball_speed * direction;
    velocity->value.y = state->ball_speed * 0.35f;
}

static int pong_overlaps(
    const ecsvm_transform_component_t *a,
    const ecsvm_transform_component_t *b
)
{
    const float a_left = a->position.x - (a->scale.x * 0.5f);
    const float a_right = a->position.x + (a->scale.x * 0.5f);
    const float a_top = a->position.y - (a->scale.y * 0.5f);
    const float a_bottom = a->position.y + (a->scale.y * 0.5f);
    const float b_left = b->position.x - (b->scale.x * 0.5f);
    const float b_right = b->position.x + (b->scale.x * 0.5f);
    const float b_top = b->position.y - (b->scale.y * 0.5f);
    const float b_bottom = b->position.y + (b->scale.y * 0.5f);

    return !(a_right < b_left || a_left > b_right || a_bottom < b_top || a_top > b_bottom);
}

static ecsvm_status_t pong_update(ecsvm_context_t *ctx)
{
    pong_state_t *state;
    int width;
    int height;
    float dt;
    int keyboard_count;
    const bool *keyboard_state;
    ecsvm_entity_t paddles[2];
    size_t index;
    ecsvm_transform_component_t *ball_transform;
    pong_velocity_component_t *ball_velocity;

    state = (pong_state_t *)ctx->api.userdata;
    if (state == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    if (!ecsvm_window_system_size(state->window_system, &width, &height)) {
        return ECSVM_ERROR_CALLBACK;
    }

    dt = ecsvm_window_system_delta_seconds(state->window_system);
    if (dt <= 0.0f || dt > 0.1f) {
        dt = 1.0f / 60.0f;
    }

    keyboard_state = SDL_GetKeyboardState(&keyboard_count);

    paddles[0] = state->left_paddle;
    paddles[1] = state->right_paddle;
    for (index = 0u; index < 2u; ++index) {
        ecsvm_transform_component_t *transform;
        const pong_paddle_component_t *paddle;
        float movement;
        const float half_height = 50.0f;
        int up_index;
        int down_index;

        transform = (ecsvm_transform_component_t *)ecsvm_component_get_mutable(
            ctx->engine,
            state->core.transform,
            paddles[index]
        );
        paddle = (const pong_paddle_component_t *)ecsvm_component_get(
            ctx->engine,
            state->paddle_component,
            paddles[index]
        );
        if (transform == NULL || paddle == NULL) {
            return ECSVM_ERROR_NOT_FOUND;
        }

        movement = 0.0f;
        up_index = (int)paddle->up_key;
        down_index = (int)paddle->down_key;
        if (keyboard_state != NULL &&
            up_index >= 0 &&
            up_index < keyboard_count &&
            keyboard_state[up_index]) {
            movement -= paddle->speed * dt;
        }
        if (keyboard_state != NULL &&
            down_index >= 0 &&
            down_index < keyboard_count &&
            keyboard_state[down_index]) {
            movement += paddle->speed * dt;
        }

        transform->position.y = pong_clamp(
            transform->position.y + movement,
            half_height,
            (float)height - half_height
        );
    }

    ball_transform = (ecsvm_transform_component_t *)ecsvm_component_get_mutable(
        ctx->engine,
        state->core.transform,
        state->ball
    );
    ball_velocity = (pong_velocity_component_t *)ecsvm_component_get_mutable(
        ctx->engine,
        state->velocity_component,
        state->ball
    );
    if (ball_transform == NULL || ball_velocity == NULL) {
        return ECSVM_ERROR_NOT_FOUND;
    }

    ball_transform->position.x += ball_velocity->value.x * dt;
    ball_transform->position.y += ball_velocity->value.y * dt;
    ball_transform->rotation += dt * 2.5f;

    if (ball_transform->position.y - (ball_transform->scale.y * 0.5f) <= 0.0f) {
        ball_transform->position.y = ball_transform->scale.y * 0.5f;
        ball_velocity->value.y = fabsf(ball_velocity->value.y);
    } else if (ball_transform->position.y + (ball_transform->scale.y * 0.5f) >= (float)height) {
        ball_transform->position.y = (float)height - (ball_transform->scale.y * 0.5f);
        ball_velocity->value.y = -fabsf(ball_velocity->value.y);
    }

    for (index = 0u; index < 2u; ++index) {
        const ecsvm_transform_component_t *paddle_transform;

        paddle_transform = (const ecsvm_transform_component_t *)ecsvm_component_get(
            ctx->engine,
            state->core.transform,
            paddles[index]
        );
        if (paddle_transform == NULL) {
            return ECSVM_ERROR_NOT_FOUND;
        }

        if (pong_overlaps(ball_transform, paddle_transform)) {
            const float offset = (ball_transform->position.y - paddle_transform->position.y) /
                (paddle_transform->scale.y * 0.5f);

            if (index == 0u) {
                ball_transform->position.x = paddle_transform->position.x +
                    (paddle_transform->scale.x * 0.5f) +
                    (ball_transform->scale.x * 0.5f);
                ball_velocity->value.x = fabsf(ball_velocity->value.x);
            } else {
                ball_transform->position.x = paddle_transform->position.x -
                    (paddle_transform->scale.x * 0.5f) -
                    (ball_transform->scale.x * 0.5f);
                ball_velocity->value.x = -fabsf(ball_velocity->value.x);
            }

            ball_velocity->value.y = offset * state->ball_speed * 0.75f;
            break;
        }
    }

    if (ball_transform->position.x < 0.0f) {
        pong_reset_ball(ctx->engine, state, width, height, 1.0f);
    } else if (ball_transform->position.x > (float)width) {
        pong_reset_ball(ctx->engine, state, width, height, -1.0f);
    }

    return ECSVM_OK;
}

static ecsvm_status_t pong_register_components(ecsvm_engine_t *engine, pong_state_t *state)
{
    ecsvm_component_desc_t desc;
    ecsvm_status_t status;

    status = ecsvm_register_core_components(engine, &state->core);
    if (status != ECSVM_OK) {
        return status;
    }

    desc.name = "pong.paddle";
    desc.size = sizeof(pong_paddle_component_t);
    desc.preferred_storage = ECSVM_STORAGE_CONTIGUOUS;
    status = ecsvm_engine_register_component(engine, &desc, &state->paddle_component);
    if (status != ECSVM_OK) {
        return status;
    }

    desc.name = "pong.velocity";
    desc.size = sizeof(pong_velocity_component_t);
    desc.preferred_storage = ECSVM_STORAGE_CONTIGUOUS;
    return ecsvm_engine_register_component(engine, &desc, &state->velocity_component);
}

static int pong_spawn_world(ecsvm_engine_t *engine, pong_state_t *state, int width, int height)
{
    ecsvm_entity_t entity;

    entity = ecsvm_entity_create(engine);
    if (entity == ECSVM_INVALID_ENTITY ||
        !pong_attach_transform(
            engine,
            state->core.transform,
            entity,
            30.0f,
            (float)height * 0.5f,
            8.0f,
            (float)height,
            0.0f
        ) ||
        !pong_attach_shape(engine, state->core.graphics_shape, entity, 0.2f, 0.2f, 0.2f, 1.0f)) {
        return 0;
    }

    entity = ecsvm_entity_create(engine);
    if (entity == ECSVM_INVALID_ENTITY ||
        !pong_attach_transform(
            engine,
            state->core.transform,
            entity,
            (float)width * 0.5f,
            (float)height * 0.5f,
            6.0f,
            18.0f,
            0.0f
        ) ||
        !pong_attach_shape(engine, state->core.graphics_shape, entity, 1.0f, 1.0f, 1.0f, 0.65f)) {
        return 0;
    }

    state->left_paddle = ecsvm_entity_create(engine);
    if (state->left_paddle == ECSVM_INVALID_ENTITY ||
        !pong_attach_transform(
            engine,
            state->core.transform,
            state->left_paddle,
            40.0f,
            (float)height * 0.5f,
            16.0f,
            100.0f,
            0.0f
        ) ||
        !pong_attach_shape(engine, state->core.graphics_shape, state->left_paddle, 1.0f, 1.0f, 1.0f, 1.0f) ||
        !pong_attach_paddle(
            engine,
            state->paddle_component,
            state->left_paddle,
            500.0f,
            SDL_SCANCODE_W,
            SDL_SCANCODE_S
        )) {
        return 0;
    }

    state->right_paddle = ecsvm_entity_create(engine);
    if (state->right_paddle == ECSVM_INVALID_ENTITY ||
        !pong_attach_transform(
            engine,
            state->core.transform,
            state->right_paddle,
            (float)width - 40.0f,
            (float)height * 0.5f,
            16.0f,
            100.0f,
            0.0f
        ) ||
        !pong_attach_shape(engine, state->core.graphics_shape, state->right_paddle, 1.0f, 1.0f, 1.0f, 1.0f) ||
        !pong_attach_paddle(
            engine,
            state->paddle_component,
            state->right_paddle,
            500.0f,
            SDL_SCANCODE_UP,
            SDL_SCANCODE_DOWN
        )) {
        return 0;
    }

    state->ball = ecsvm_entity_create(engine);
    if (state->ball == ECSVM_INVALID_ENTITY ||
        !pong_attach_transform(
            engine,
            state->core.transform,
            state->ball,
            (float)width * 0.5f,
            (float)height * 0.5f,
            16.0f,
            16.0f,
            0.0f
        ) ||
        !pong_attach_shape(engine, state->core.graphics_shape, state->ball, 1.0f, 0.85f, 0.2f, 1.0f) ||
        !pong_attach_velocity(engine, state->velocity_component, state->ball, state->ball_speed, state->ball_speed * 0.35f)) {
        return 0;
    }

    return 1;
}

int ecsvm_run_pong(void)
{
    ecsvm_engine_t *engine;
    ecsvm_window_config_t window_config;
    ecsvm_renderer_config_t renderer_config;
    ecsvm_window_system_t *window_system;
    ecsvm_renderer_system_t *renderer_system;
    ecsvm_system_desc_t update_system;
    pong_state_t state;
    ecsvm_status_t status;
    int exit_code;

    memset(&state, 0, sizeof(state));
    state.ball_speed = 420.0f;

    window_config.title = "ecsvm pong";
    window_config.width = 960;
    window_config.height = 540;
    window_config.window_flags = 0;

    engine = ecsvm_engine_create();
    if (engine == NULL) {
        fprintf(stderr, "failed to create engine\n");
        return 1;
    }

    window_system = ecsvm_window_system_create(&window_config);
    if (window_system == NULL) {
        fprintf(stderr, "failed to create window system\n");
        ecsvm_engine_destroy(engine);
        return 1;
    }

    status = pong_register_components(engine, &state);
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to register pong components: %s\n", ecsvm_status_string(status));
        ecsvm_window_system_destroy(window_system);
        ecsvm_engine_destroy(engine);
        return 1;
    }

    state.window_system = window_system;
    if (!pong_spawn_world(engine, &state, window_config.width, window_config.height)) {
        fprintf(stderr, "failed to create pong world\n");
        ecsvm_window_system_destroy(window_system);
        ecsvm_engine_destroy(engine);
        return 1;
    }

    renderer_config.components = state.core;
    renderer_config.clear_color.x = 0.05f;
    renderer_config.clear_color.y = 0.07f;
    renderer_config.clear_color.z = 0.10f;
    renderer_config.clear_color.w = 1.0f;
    renderer_system = ecsvm_renderer_system_create(window_system, &renderer_config);
    if (renderer_system == NULL) {
        fprintf(stderr, "failed to create renderer system\n");
        ecsvm_window_system_destroy(window_system);
        ecsvm_engine_destroy(engine);
        return 1;
    }

    status = ecsvm_window_system_register(engine, window_system);
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to register window system: %s\n", ecsvm_status_string(status));
        ecsvm_renderer_system_destroy(renderer_system);
        ecsvm_window_system_destroy(window_system);
        ecsvm_engine_destroy(engine);
        return 1;
    }

    memset(&update_system, 0, sizeof(update_system));
    update_system.name = "pong.update";
    update_system.callback = pong_update;
    update_system.user_data = &state;
    status = ecsvm_engine_register_system(engine, &update_system, NULL);
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to register pong update system: %s\n", ecsvm_status_string(status));
        ecsvm_renderer_system_destroy(renderer_system);
        ecsvm_window_system_destroy(window_system);
        ecsvm_engine_destroy(engine);
        return 1;
    }

    status = ecsvm_renderer_system_register(engine, renderer_system);
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to register renderer system: %s\n", ecsvm_status_string(status));
        ecsvm_renderer_system_destroy(renderer_system);
        ecsvm_window_system_destroy(window_system);
        ecsvm_engine_destroy(engine);
        return 1;
    }

    exit_code = 0;
    while (!ecsvm_window_system_should_close(window_system)) {
        status = ecsvm_engine_run(engine);
        if (status != ECSVM_OK) {
            fprintf(stderr, "pong runtime failed: %s\n", ecsvm_status_string(status));
            exit_code = 1;
            break;
        }
        SDL_Delay(1);
    }

    ecsvm_renderer_system_destroy(renderer_system);
    ecsvm_window_system_destroy(window_system);
    ecsvm_engine_destroy(engine);
    return exit_code;
}
