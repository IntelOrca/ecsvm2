#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define _POSIX_C_SOURCE 200809L
#include <time.h>
#endif

#include "ecsvm/system.h"

#include <string.h>

typedef struct ecsvm_time_system_state {
    ecsvm_component_id_t time_component;
    ecsvm_entity_t time_entity;
    uint64_t previous_tick_ns;
    int has_previous_tick;
} ecsvm_time_system_state_t;

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

static ecsvm_time_system_state_t *ecsvm_time_system_state(ecsvm_engine_t *engine)
{
    ecsvm_system_t *system;

    if (engine == NULL) {
        return NULL;
    }

    system = engine->current_system(engine);
    return system != NULL
        ? (ecsvm_time_system_state_t *)system->get_userdata(system)
        : NULL;
}

static ecsvm_status_t ecsvm_time_system_main(ecsvm_engine_t *engine)
{
    ecsvm_time_system_state_t *system;
    ecsvm_time_component_t *time_component;
    uint64_t now_ns;
    ecsvm_status_t status;

    system = ecsvm_time_system_state(engine);
    if (engine == NULL || system == NULL || system->time_component == ECSVM_INVALID_COMPONENT) {
        return ECSVM_ERROR_ARGUMENT;
    }

    if (system->time_entity == ECSVM_INVALID_ENTITY) {
        ecsvm_time_component_t initial_time;

        memset(&initial_time, 0, sizeof(initial_time));
        system->time_entity = ecsvm_entity_create(engine);
        if (system->time_entity == ECSVM_INVALID_ENTITY) {
            return ECSVM_ERROR_MEMORY;
        }

        status = ecsvm_component_set(
            engine,
            system->time_component,
            system->time_entity,
            &initial_time
        );
        if (status != ECSVM_OK) {
            return status;
        }
    }

    time_component = (ecsvm_time_component_t *)ecsvm_component_get_mutable(
        engine,
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

static void ecsvm_time_system_dispose(ecsvm_engine_t *engine, ecsvm_system_t *system)
{
    ecsvm_time_system_state_t *state;

    if (engine == NULL || system == NULL) {
        return;
    }

    state = (ecsvm_time_system_state_t *)system->get_userdata(system);
    if (state != NULL) {
        engine->free(engine, state);
    }
}

ecsvm_status_t ecsvm_system_time_register(ecsvm_engine_t *engine)
{
    ecsvm_system_definition_t definition;
    ecsvm_time_system_state_t *system;
    ecsvm_status_t status;

    if (engine == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    system = (ecsvm_time_system_state_t *)engine->alloc(engine, sizeof(*system));
    if (system == NULL) {
        return ECSVM_ERROR_MEMORY;
    }

    memset(system, 0, sizeof(*system));
    system->time_component = ecsvm_engine_find_component(engine, ECSVM_SYSTEM_TIME_NAME);
    system->time_entity = ECSVM_INVALID_ENTITY;
    if (system->time_component == ECSVM_INVALID_COMPONENT) {
        engine->free(engine, system);
        return ECSVM_ERROR_ARGUMENT;
    }

    memset(&definition, 0, sizeof(definition));
    definition.name = ECSVM_SYSTEM_TIME_NAME;
    definition.main = ecsvm_time_system_main;
    definition.dispose = ecsvm_time_system_dispose;
    definition.userdata = system;
    status = engine->register_system(engine, &definition);
    if (status != ECSVM_OK) {
        engine->free(engine, system);
        return status;
    }

    return ECSVM_OK;
}
