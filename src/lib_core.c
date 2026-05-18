#include "ecsvm_internal.h"

#include <stdio.h>
#include <string.h>

typedef struct ecsvm_native_binding {
    const char *qualified_name;
    ecsvm_native_function_fn callback;
} ecsvm_native_binding_t;

static ecsvm_managed_value_t ecsvm_managed_void_value(void)
{
    ecsvm_managed_value_t value;
    memset(&value, 0, sizeof(value));
    value.kind = ECSVM_MANAGED_VALUE_VOID;
    return value;
}

static ecsvm_managed_value_t ecsvm_managed_null_value(void)
{
    ecsvm_managed_value_t value;
    memset(&value, 0, sizeof(value));
    value.kind = ECSVM_MANAGED_VALUE_NULL;
    return value;
}

static int ecsvm_managed_number_argument(
    const ecsvm_managed_value_t *arguments,
    size_t argument_count,
    size_t index,
    double *out_value
)
{
    if (arguments == NULL ||
        out_value == NULL ||
        index >= argument_count ||
        arguments[index].kind != ECSVM_MANAGED_VALUE_NUMBER) {
        return 0;
    }

    *out_value = arguments[index].number_value;
    return 1;
}

static void ecsvm_core_print_value(
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_managed_value_t *value
)
{
    switch (value->kind) {
        case ECSVM_MANAGED_VALUE_VOID:
            fputs("void", stdout);
            break;
        case ECSVM_MANAGED_VALUE_NULL:
            fputs("null", stdout);
            break;
        case ECSVM_MANAGED_VALUE_BOOL:
            fputs(value->boolean_value ? "true" : "false", stdout);
            break;
        case ECSVM_MANAGED_VALUE_NUMBER:
            fprintf(stdout, "%g", value->number_value);
            break;
        case ECSVM_MANAGED_VALUE_STRING: {
            const ecsvm_ecsbin_blob_t *blob;

            blob = ecsvm_ecsbin_blob_ref(module, value->blob_id);
            if (blob != NULL &&
                blob->length >= 2u &&
                blob->data[0] == '"' &&
                blob->data[blob->length - 1u] == '"') {
                fwrite(blob->data + 1u, 1u, (size_t)blob->length - 2u, stdout);
            } else if (blob != NULL) {
                fwrite(blob->data, 1u, (size_t)blob->length, stdout);
            }
            break;
        }
        default:
            break;
    }

    fputc('\n', stdout);
    fflush(stdout);
}

static ecsvm_status_t ecsvm_core_print(
    ecsvm_engine_t *engine,
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_function_ref_t *function_ref,
    uint32_t type_argument_id,
    const ecsvm_managed_value_t *arguments,
    size_t argument_count,
    ecsvm_managed_value_t *out_value
)
{
    (void)engine;
    (void)function_ref;
    (void)type_argument_id;

    if (module == NULL || out_value == NULL || argument_count != 1u) {
        return ECSVM_ERROR_ARGUMENT;
    }

    ecsvm_core_print_value(module, &arguments[0]);
    *out_value = ecsvm_managed_void_value();
    return ECSVM_OK;
}

static ecsvm_status_t ecsvm_core_stop(
    ecsvm_engine_t *engine,
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_function_ref_t *function_ref,
    uint32_t type_argument_id,
    const ecsvm_managed_value_t *arguments,
    size_t argument_count,
    ecsvm_managed_value_t *out_value
)
{
    (void)module;
    (void)function_ref;
    (void)type_argument_id;
    (void)arguments;

    if (engine == NULL || out_value == NULL || argument_count != 0u) {
        return ECSVM_ERROR_ARGUMENT;
    }

    ecsvm_engine_request_stop(engine);
    *out_value = ecsvm_managed_void_value();
    return ECSVM_OK;
}

static ecsvm_status_t ecsvm_core_math_min(
    ecsvm_engine_t *engine,
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_function_ref_t *function_ref,
    uint32_t type_argument_id,
    const ecsvm_managed_value_t *arguments,
    size_t argument_count,
    ecsvm_managed_value_t *out_value
)
{
    double left;
    double right;

    (void)engine;
    (void)module;
    (void)function_ref;
    (void)type_argument_id;

    if (out_value == NULL ||
        argument_count != 2u ||
        !ecsvm_managed_number_argument(arguments, argument_count, 0u, &left) ||
        !ecsvm_managed_number_argument(arguments, argument_count, 1u, &right)) {
        return ECSVM_ERROR_ARGUMENT;
    }

    out_value->kind = ECSVM_MANAGED_VALUE_NUMBER;
    out_value->number_value = left < right ? left : right;
    return ECSVM_OK;
}

static ecsvm_status_t ecsvm_core_math_max(
    ecsvm_engine_t *engine,
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_function_ref_t *function_ref,
    uint32_t type_argument_id,
    const ecsvm_managed_value_t *arguments,
    size_t argument_count,
    ecsvm_managed_value_t *out_value
)
{
    double left;
    double right;

    (void)engine;
    (void)module;
    (void)function_ref;
    (void)type_argument_id;

    if (out_value == NULL ||
        argument_count != 2u ||
        !ecsvm_managed_number_argument(arguments, argument_count, 0u, &left) ||
        !ecsvm_managed_number_argument(arguments, argument_count, 1u, &right)) {
        return ECSVM_ERROR_ARGUMENT;
    }

    out_value->kind = ECSVM_MANAGED_VALUE_NUMBER;
    out_value->number_value = left > right ? left : right;
    return ECSVM_OK;
}

static ecsvm_status_t ecsvm_core_math_clamp(
    ecsvm_engine_t *engine,
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_function_ref_t *function_ref,
    uint32_t type_argument_id,
    const ecsvm_managed_value_t *arguments,
    size_t argument_count,
    ecsvm_managed_value_t *out_value
)
{
    double value;
    double minimum;
    double maximum;

    (void)engine;
    (void)module;
    (void)function_ref;
    (void)type_argument_id;

    if (out_value == NULL ||
        argument_count != 3u ||
        !ecsvm_managed_number_argument(arguments, argument_count, 0u, &value) ||
        !ecsvm_managed_number_argument(arguments, argument_count, 1u, &minimum) ||
        !ecsvm_managed_number_argument(arguments, argument_count, 2u, &maximum)) {
        return ECSVM_ERROR_ARGUMENT;
    }

    if (value < minimum) {
        value = minimum;
    }
    if (value > maximum) {
        value = maximum;
    }

    out_value->kind = ECSVM_MANAGED_VALUE_NUMBER;
    out_value->number_value = value;
    return ECSVM_OK;
}

static ecsvm_status_t ecsvm_core_spawn(
    ecsvm_engine_t *engine,
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_function_ref_t *function_ref,
    uint32_t type_argument_id,
    const ecsvm_managed_value_t *arguments,
    size_t argument_count,
    ecsvm_managed_value_t *out_value
)
{
    ecsvm_entity_t entity;

    (void)module;
    (void)function_ref;
    (void)type_argument_id;
    (void)arguments;

    if (engine == NULL || out_value == NULL || argument_count != 0u) {
        return ECSVM_ERROR_ARGUMENT;
    }

    entity = ecsvm_entity_create(engine);
    if (entity == ECSVM_INVALID_ENTITY) {
        return ECSVM_ERROR_MEMORY;
    }

    out_value->kind = ECSVM_MANAGED_VALUE_NUMBER;
    out_value->number_value = (double)entity;
    return ECSVM_OK;
}

static ecsvm_status_t ecsvm_core_get_first_entity(
    ecsvm_engine_t *engine,
    const ecsvm_ecsbin_module_t *module,
    const ecsvm_ecsbin_function_ref_t *function_ref,
    uint32_t type_argument_id,
    const ecsvm_managed_value_t *arguments,
    size_t argument_count,
    ecsvm_managed_value_t *out_value
)
{
    const ecsvm_ecsbin_type_ref_t *type_ref;
    ecsvm_component_id_t component_id;
    size_t index;

    (void)function_ref;
    (void)arguments;

    if (engine == NULL ||
        module == NULL ||
        out_value == NULL ||
        argument_count != 0u ||
        type_argument_id == 0u) {
        return ECSVM_ERROR_ARGUMENT;
    }

    type_ref = ecsvm_ecsbin_type_ref(module, type_argument_id);
    if (type_ref == NULL || type_ref->qualified_name == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    component_id = ecsvm_engine_find_component(engine, type_ref->qualified_name);
    if (component_id == ECSVM_INVALID_COMPONENT) {
        *out_value = ecsvm_managed_null_value();
        return ECSVM_OK;
    }

    for (index = 0u; index < ecsvm_entity_count(engine); ++index) {
        ecsvm_entity_t entity;

        entity = ecsvm_entity_at(engine, index);
        if (entity != ECSVM_INVALID_ENTITY &&
            ecsvm_component_has(engine, component_id, entity)) {
            out_value->kind = ECSVM_MANAGED_VALUE_NUMBER;
            out_value->number_value = (double)entity;
            return ECSVM_OK;
        }
    }

    *out_value = ecsvm_managed_null_value();
    return ECSVM_OK;
}

ecsvm_native_function_fn ecsvm_core_find_native_function(const char *qualified_name)
{
    static const ecsvm_native_binding_t bindings[] = {
        { "core.Print", ecsvm_core_print },
        { "core.print", ecsvm_core_print },
        { "core.Stop", ecsvm_core_stop },
        { "core.stop", ecsvm_core_stop },
        { "core.spawn", ecsvm_core_spawn },
        { "core.getFirstEntity", ecsvm_core_get_first_entity },
        { "core.math.min", ecsvm_core_math_min },
        { "core.math.max", ecsvm_core_math_max },
        { "core.math.clamp", ecsvm_core_math_clamp }
    };
    size_t index;

    if (qualified_name == NULL) {
        return NULL;
    }

    for (index = 0u; index < sizeof(bindings) / sizeof(bindings[0]); ++index) {
        if (strcmp(bindings[index].qualified_name, qualified_name) == 0) {
            return bindings[index].callback;
        }
    }

    return NULL;
}
