#include "ecsvm/component.h"
#include "ecsvm/project.h"
#include "ecsvm/ecsvm.h"

#include <stdio.h>
#include <string.h>

typedef struct vec3 {
    float x;
    float y;
    float z;
} vec3_t;

typedef struct demo_components {
    ecsvm_core_components_t core;
    ecsvm_component_id_t position;
    ecsvm_component_id_t velocity;
} demo_components_t;

static demo_components_t g_demo_components;

int ecsvm_run_pong(void);
int ecsvm_run_pong_binary(const char *ecsbin_path);

static ecsvm_status_t demo_gravity(ecsvm_context_t *ctx)
{
    const float gravity = -9.8f;
    size_t index;

    for (index = 0u; index < ecsvm_entity_count(ctx->engine); ++index) {
        ecsvm_entity_t entity;
        vec3_t *velocity;

        entity = ecsvm_entity_at(ctx->engine, index);
        if (!ecsvm_component_has(ctx->engine, g_demo_components.velocity, entity)) {
            continue;
        }

        velocity = (vec3_t *)ecsvm_component_get_mutable(
            ctx->engine,
            g_demo_components.velocity,
            entity
        );
        if (velocity == NULL) {
            return ECSVM_ERROR_NOT_FOUND;
        }

        velocity->y += gravity;
    }

    ctx->api.log(ctx->api.userdata, "gravity complete");
    return ECSVM_OK;
}

static ecsvm_status_t demo_integrate(ecsvm_context_t *ctx)
{
    size_t index;

    for (index = 0u; index < ecsvm_entity_count(ctx->engine); ++index) {
        ecsvm_entity_t entity;
        vec3_t *position;
        const vec3_t *velocity;

        entity = ecsvm_entity_at(ctx->engine, index);
        if (!ecsvm_component_has(ctx->engine, g_demo_components.position, entity) ||
            !ecsvm_component_has(ctx->engine, g_demo_components.velocity, entity)) {
            continue;
        }

        position = (vec3_t *)ecsvm_component_get_mutable(
            ctx->engine,
            g_demo_components.position,
            entity
        );
        velocity = (const vec3_t *)ecsvm_component_get(
            ctx->engine,
            g_demo_components.velocity,
            entity
        );
        if (position == NULL || velocity == NULL) {
            return ECSVM_ERROR_NOT_FOUND;
        }

        position->x += velocity->x;
        position->y += velocity->y;
        position->z += velocity->z;
    }

    ctx->api.log(ctx->api.userdata, "integrate complete");
    return ECSVM_OK;
}

static void print_usage(const char *argv0)
{
    fprintf(
        stderr,
        "usage: %s --self-test | --pong | build <project> | run <project|ecsbin>\n",
        argv0
    );
}

static int run_self_test(void)
{
    ecsvm_component_desc_t position_desc;
    ecsvm_component_desc_t velocity_desc;
    ecsvm_system_desc_t gravity_desc;
    ecsvm_system_desc_t integrate_desc;
    ecsvm_engine_t *engine;
    ecsvm_hierarchy_component_t hierarchy;
    vec3_t position;
    vec3_t velocity;
    ecsvm_entity_t entity;
    ecsvm_blob_t string_blob;
    ecsvm_status_t status;

    engine = ecsvm_engine_create();
    if (engine == NULL) {
        fprintf(stderr, "failed to create engine\n");
        return 1;
    }

    status = ecsvm_register_core_components(engine, &g_demo_components.core);
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to register built-ins: %s\n", ecsvm_status_string(status));
        ecsvm_engine_destroy(engine);
        return 1;
    }

    position_desc.name = "app.transform";
    position_desc.size = sizeof(position);
    position_desc.preferred_storage = ECSVM_STORAGE_CONTIGUOUS;
    velocity_desc.name = "app.velocity";
    velocity_desc.size = sizeof(velocity);
    velocity_desc.preferred_storage = ECSVM_STORAGE_CONTIGUOUS;

    status = ecsvm_engine_register_component(engine, &position_desc, &g_demo_components.position);
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to register position: %s\n", ecsvm_status_string(status));
        ecsvm_engine_destroy(engine);
        return 1;
    }

    status = ecsvm_engine_register_component(engine, &velocity_desc, &g_demo_components.velocity);
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to register velocity: %s\n", ecsvm_status_string(status));
        ecsvm_engine_destroy(engine);
        return 1;
    }

    gravity_desc.name = "app.gravity";
    gravity_desc.callback = demo_gravity;
    gravity_desc.alloc = NULL;
    gravity_desc.free = NULL;
    gravity_desc.log = NULL;
    gravity_desc.user_data = NULL;

    integrate_desc.name = "app.integrate";
    integrate_desc.callback = demo_integrate;
    integrate_desc.alloc = NULL;
    integrate_desc.free = NULL;
    integrate_desc.log = NULL;
    integrate_desc.user_data = NULL;

    status = ecsvm_engine_register_system(engine, &gravity_desc, NULL);
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to register gravity: %s\n", ecsvm_status_string(status));
        ecsvm_engine_destroy(engine);
        return 1;
    }

    status = ecsvm_engine_register_system(engine, &integrate_desc, NULL);
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to register integrate: %s\n", ecsvm_status_string(status));
        ecsvm_engine_destroy(engine);
        return 1;
    }

    entity = ecsvm_entity_create(engine);
    if (entity == ECSVM_INVALID_ENTITY) {
        fprintf(stderr, "failed to create entity\n");
        ecsvm_engine_destroy(engine);
        return 1;
    }

    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;

    velocity.x = 1.0f;
    velocity.y = 10.0f;
    velocity.z = 0.0f;

    hierarchy.parent = ECSVM_INVALID_ENTITY;
    hierarchy.first_child = ECSVM_INVALID_ENTITY;
    hierarchy.next_sibling = ECSVM_INVALID_ENTITY;
    hierarchy.prev_sibling = ECSVM_INVALID_ENTITY;

    status = ecsvm_component_set(engine, g_demo_components.position, entity, &position);
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to attach position: %s\n", ecsvm_status_string(status));
        ecsvm_engine_destroy(engine);
        return 1;
    }

    status = ecsvm_component_set(engine, g_demo_components.velocity, entity, &velocity);
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to attach velocity: %s\n", ecsvm_status_string(status));
        ecsvm_engine_destroy(engine);
        return 1;
    }

    status = ecsvm_component_set(
        engine,
        g_demo_components.core.hierarchy,
        entity,
        &hierarchy
    );
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to attach hierarchy: %s\n", ecsvm_status_string(status));
        ecsvm_engine_destroy(engine);
        return 1;
    }

    status = ecsvm_string_create(engine, "ecsvm self-test", &string_blob);
    if (status != ECSVM_OK) {
        fprintf(stderr, "failed to allocate string: %s\n", ecsvm_status_string(status));
        ecsvm_engine_destroy(engine);
        return 1;
    }

    status = ecsvm_engine_run(engine);
    if (status != ECSVM_OK) {
        fprintf(stderr, "runtime failed: %s\n", ecsvm_status_string(status));
        ecsvm_engine_destroy(engine);
        return 1;
    }

    position = *(const vec3_t *)ecsvm_component_get(engine, g_demo_components.position, entity);
    printf(
        "self-test ok: entity=%u systems=%zu position=(%.2f, %.2f, %.2f) message=\"%s\"\n",
        (unsigned)entity,
        ecsvm_engine_system_count(engine),
        position.x,
        position.y,
        position.z,
        ecsvm_string_cstr(engine, string_blob)
    );

    ecsvm_engine_destroy(engine);
    return 0;
}

int main(int argc, char **argv)
{
    char output_path[512];
    char error_message[512];

    if (argc == 2 && strcmp(argv[1], "--self-test") == 0) {
        return run_self_test();
    }

    if (argc == 2 && strcmp(argv[1], "--pong") == 0) {
        return ecsvm_run_pong();
    }

    if (argc == 3 && strcmp(argv[1], "build") == 0) {
        ecsvm_status_t status;

        status = ecsvm_project_build(
            argv[2],
            output_path,
            sizeof(output_path),
            error_message,
            sizeof(error_message)
        );
        if (status != ECSVM_OK) {
            fprintf(stderr, "build failed: %s\n", error_message[0] != '\0' ? error_message : ecsvm_status_string(status));
            return 1;
        }

        printf("%s\n", output_path);
        return 0;
    }

    if (argc == 3 && strcmp(argv[1], "run") == 0) {
        const char *binary_path;

        if (ecsvm_path_is_directory(argv[2])) {
            ecsvm_status_t status;

            status = ecsvm_project_build(
                argv[2],
                output_path,
                sizeof(output_path),
                error_message,
                sizeof(error_message)
            );
            if (status != ECSVM_OK) {
                fprintf(stderr, "build failed: %s\n", error_message[0] != '\0' ? error_message : ecsvm_status_string(status));
                return 1;
            }
            binary_path = output_path;
        } else if (ecsvm_path_has_extension(argv[2], ".ecsbin")) {
            binary_path = argv[2];
        } else {
            fprintf(stderr, "run expects a project directory or .ecsbin file\n");
            return 1;
        }

        return ecsvm_run_pong_binary(binary_path);
    }

    print_usage(argv[0]);
    return 1;
}
