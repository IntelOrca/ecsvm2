#include "ecsvm/component.h"

#include <string.h>

ecsvm_status_t ecsvm_register_core_components(
    ecsvm_engine_t *engine,
    ecsvm_core_components_t *out_components
)
{
    ecsvm_component_desc_t desc;
    ecsvm_core_components_t components;
    ecsvm_status_t status;

    if (engine == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    memset(&components, 0, sizeof(components));

    status = ecsvm_engine_register_builtin_components(engine);
    if (status != ECSVM_OK) {
        return status;
    }

    components.hierarchy = ecsvm_engine_hierarchy_component(engine);

    desc.name = "core.Transform";
    desc.size = sizeof(ecsvm_transform_component_t);
    desc.preferred_storage = ECSVM_STORAGE_CONTIGUOUS;
    status = ecsvm_engine_register_component(engine, &desc, &components.transform);
    if (status != ECSVM_OK) {
        return status;
    }

    desc.name = "core.Time";
    desc.size = sizeof(ecsvm_time_component_t);
    desc.preferred_storage = ECSVM_STORAGE_CONTIGUOUS;
    status = ecsvm_engine_register_component(engine, &desc, &components.time);
    if (status != ECSVM_OK) {
        return status;
    }

    desc.name = "core.graphics.GraphicsShape";
    desc.size = sizeof(ecsvm_graphics_shape_component_t);
    desc.preferred_storage = ECSVM_STORAGE_CONTIGUOUS;
    status = ecsvm_engine_register_component(engine, &desc, &components.graphics_shape);
    if (status != ECSVM_OK) {
        return status;
    }

    desc.name = "core.input.InputMonitor";
    desc.size = sizeof(ecsvm_input_monitor_component_t);
    desc.preferred_storage = ECSVM_STORAGE_CONTIGUOUS;
    status = ecsvm_engine_register_component(engine, &desc, &components.input_monitor);
    if (status != ECSVM_OK) {
        return status;
    }

    desc.name = "core.ui.Window";
    desc.size = sizeof(ecsvm_window_component_t);
    desc.preferred_storage = ECSVM_STORAGE_CONTIGUOUS;
    status = ecsvm_engine_register_component(engine, &desc, &components.window);
    if (status != ECSVM_OK) {
        return status;
    }

    if (out_components != NULL) {
        *out_components = components;
    }

    return ECSVM_OK;
}
