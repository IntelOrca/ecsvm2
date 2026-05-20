#include "system_hotreload.h"

#include <stdio.h>
#include <string.h>

static ecsvm_status_t ecsvm_hotreload_system_tick(ecsvm_context_t *ctx)
{
    ecsvm_hotreload_system_t *system;
    char message[512];
    int changed;

    if (ctx == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    system = (ecsvm_hotreload_system_t *)ctx->api.userdata;
    if (system == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    changed = 0;
    if (!ecsvm_fswatch_poll(&system->watcher, &changed, message, sizeof(message))) {
        if (message[0] != '\0') {
            (void)snprintf(message, sizeof(message), "hotreload: %s", message);
            ctx->api.log(ctx->api.userdata, message);
        }
        return ECSVM_OK;
    }

    if (changed && !system->reload_requested) {
        ctx->api.log(ctx->api.userdata, "hotreload: source change detected");
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
    ecsvm_system_desc_t desc;

    if (engine == NULL || system == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    memset(&desc, 0, sizeof(desc));
    desc.name = "__ecsvm.HotReload";
    desc.callback = ecsvm_hotreload_system_tick;
    desc.user_data = system;
    return ecsvm_engine_register_system(engine, &desc, NULL);
}
