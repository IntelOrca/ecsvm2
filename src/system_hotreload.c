#include "system_hotreload.h"

#include <stdio.h>
#include <string.h>

static ecsvm_status_t ecsvm_hotreload_system_tick(ecsvm_engine_t *engine)
{
    ecsvm_hotreload_system_t *system;
    ecsvm_system_t *current_system;
    char message[512];
    int changed;

    if (engine == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    current_system = engine->current_system(engine);
    system = current_system != NULL
        ? (ecsvm_hotreload_system_t *)current_system->get_userdata(current_system)
        : NULL;
    if (system == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    changed = 0;
    if (!ecsvm_fswatch_poll(&system->watcher, &changed, message, sizeof(message))) {
        if (message[0] != '\0') {
            (void)snprintf(message, sizeof(message), "hotreload: %s", message);
            engine->log(engine, message);
        }
        return ECSVM_OK;
    }

    if (changed && !system->reload_requested) {
        engine->log(engine, "hotreload: source change detected");
        system->reload_requested = 1;
    }

    return ECSVM_OK;
}

int ecsvm_hotreload_system_init(
    ecsvm_hotreload_system_t *system,
    const char *project_path,
    char *error_message,
    size_t error_message_capacity
)
{
    if (system == NULL) {
        return 0;
    }

    memset(system, 0, sizeof(*system));
    return ecsvm_fswatch_init(&system->watcher, project_path, error_message, error_message_capacity);
}

void ecsvm_hotreload_system_free(ecsvm_hotreload_system_t *system)
{
    if (system == NULL) {
        return;
    }

    ecsvm_fswatch_free(&system->watcher);
    memset(system, 0, sizeof(*system));
}

int ecsvm_hotreload_system_consume_reload(ecsvm_hotreload_system_t *system)
{
    int requested;

    if (system == NULL) {
        return 0;
    }

    requested = system->reload_requested;
    system->reload_requested = 0;
    return requested;
}

ecsvm_status_t ecsvm_hotreload_system_register(
    ecsvm_engine_t *engine,
    ecsvm_hotreload_system_t *system
)
{
    ecsvm_system_definition_t definition;

    if (engine == NULL || system == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    memset(&definition, 0, sizeof(definition));
    definition.name = "__ecsvm.HotReload";
    definition.main = ecsvm_hotreload_system_tick;
    definition.userdata = system;
    return ecsvm_engine_register_system(engine, &definition);
}
