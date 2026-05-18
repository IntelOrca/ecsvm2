#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define _POSIX_C_SOURCE 200809L
#include <time.h>
#endif

#include "ecsvm/system_time.h"

#include <string.h>

static uint64_t ecsvm_time_system_now_ns(void)
{
#ifdef _WIN32
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;

    if (!QueryPerformanceCounter(&counter) ||
        !QueryPerformanceFrequency(&frequency) ||
        frequency.QuadPart == 0) {
        return 0u;
    }

    return (uint64_t)(
        ((unsigned long long)counter.QuadPart * 1000000000ull) /
        (unsigned long long)frequency.QuadPart
    );
#else
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0u;
    }

    return ((uint64_t)ts.tv_sec * 1000000000ull) + (uint64_t)ts.tv_nsec;
#endif
}

static ecsvm_status_t ecsvm_time_system_tick(ecsvm_context_t *ctx)
{
    ecsvm_time_system_t *system;
    ecsvm_time_component_t *time_component;
    uint64_t now_ns;
    ecsvm_status_t status;

    system = (ecsvm_time_system_t *)ctx->api.userdata;
    if (ctx == NULL || system == NULL || system->time_component == ECSVM_INVALID_COMPONENT) {
        return ECSVM_ERROR_ARGUMENT;
    }

    if (system->time_entity == ECSVM_INVALID_ENTITY) {
        ecsvm_time_component_t initial_time;

        memset(&initial_time, 0, sizeof(initial_time));
        system->time_entity = ecsvm_entity_create(ctx->engine);
        if (system->time_entity == ECSVM_INVALID_ENTITY) {
            return ECSVM_ERROR_MEMORY;
        }

        status = ecsvm_component_set(
            ctx->engine,
            system->time_component,
            system->time_entity,
            &initial_time
        );
        if (status != ECSVM_OK) {
            return status;
        }
    }

    time_component = (ecsvm_time_component_t *)ecsvm_component_get_mutable(
        ctx->engine,
        system->time_component,
        system->time_entity
    );
    if (time_component == NULL) {
        return ECSVM_ERROR_NOT_FOUND;
    }

    now_ns = ecsvm_time_system_now_ns();
    time_component->last_tick_duration_ns = system->has_previous_tick
        ? now_ns - system->previous_tick_ns
        : 0u;
    time_component->tick_count += 1u;
    system->previous_tick_ns = now_ns;
    system->has_previous_tick = 1;
    return ECSVM_OK;
}

void ecsvm_time_system_init(
    ecsvm_time_system_t *system,
    ecsvm_component_id_t time_component
)
{
    if (system == NULL) {
        return;
    }

    memset(system, 0, sizeof(*system));
    system->time_component = time_component;
}

ecsvm_status_t ecsvm_time_system_register(
    ecsvm_engine_t *engine,
    ecsvm_time_system_t *system
)
{
    ecsvm_system_desc_t desc;

    if (engine == NULL || system == NULL || system->time_component == ECSVM_INVALID_COMPONENT) {
        return ECSVM_ERROR_ARGUMENT;
    }

    memset(&desc, 0, sizeof(desc));
    desc.name = "core.time";
    desc.callback = ecsvm_time_system_tick;
    desc.user_data = system;
    return ecsvm_engine_register_system(engine, &desc, NULL);
}
