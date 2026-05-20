#include "ecsvm_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ecsvm_component_store {
    char *name;
    size_t size;
    ecsvm_storage_mode_t storage_mode;
    ecsvm_entity_t *entities;
    unsigned char *data;
    size_t count;
    size_t capacity;
} ecsvm_component_store_t;

typedef struct ecsvm_blob_slot {
    unsigned char *data;
    size_t size;
    int in_use;
    int is_string;
} ecsvm_blob_slot_t;

struct ecsvm_engine {
    ecsvm_entity_t next_entity_id;
    ecsvm_entity_t *entities;
    size_t entity_count;
    size_t entity_capacity;

    ecsvm_component_store_t *components;
    size_t component_count;
    size_t component_capacity;

    ecsvm_system_entry_t *systems;
    size_t system_count;
    size_t system_capacity;
    size_t *system_pipeline;
    size_t system_pipeline_count;
    int pipeline_dirty;

    ecsvm_function_entry_t *functions;
    size_t function_count;
    size_t function_capacity;

    ecsvm_blob_slot_t *blobs;
    size_t blob_count;
    size_t blob_capacity;

    ecsvm_component_id_t hierarchy_component;
    int stop_requested;
};

static void *ecsvm_default_alloc(void *userdata, size_t size)
{
    (void)userdata;
    return malloc(size);
}

static void ecsvm_default_free(void *userdata, void *ptr)
{
    (void)userdata;
    free(ptr);
}

static void ecsvm_default_log(void *userdata, const char *message)
{
    (void)userdata;
    if (message != NULL) {
        fprintf(stderr, "%s\n", message);
    }
}

static size_t ecsvm_next_capacity(size_t current, size_t minimum)
{
    size_t capacity;

    capacity = current == 0u ? 4u : current;
    while (capacity < minimum) {
        capacity *= 2u;
    }

    return capacity;
}

static char *ecsvm_copy_string(const char *text)
{
    char *copy;
    size_t length;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1u);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1u);
    return copy;
}

static void ecsvm_free_string_array(char **items, size_t count)
{
    size_t index;

    if (items == NULL) {
        return;
    }

    for (index = 0u; index < count; ++index) {
        free(items[index]);
    }
    free(items);
}

static char **ecsvm_copy_string_array(
    const char *const *items,
    size_t count
)
{
    char **copies;
    size_t index;

    if (count == 0u) {
        return NULL;
    }
    if (items == NULL) {
        return NULL;
    }

    copies = (char **)calloc(count, sizeof(*copies));
    if (copies == NULL) {
        return NULL;
    }

    for (index = 0u; index < count; ++index) {
        copies[index] = ecsvm_copy_string(items[index]);
        if (copies[index] == NULL) {
            ecsvm_free_string_array(copies, count);
            return NULL;
        }
    }

    return copies;
}

static void ecsvm_free_system_entry(ecsvm_system_entry_t *entry)
{
    if (entry == NULL) {
        return;
    }

    free(entry->name);
    ecsvm_free_string_array(entry->before, entry->before_count);
    ecsvm_free_string_array(entry->after, entry->after_count);
    memset(entry, 0, sizeof(*entry));
}

static ecsvm_status_t ecsvm_engine_ensure_pipeline(ecsvm_engine_t *engine)
{
    size_t *order;
    size_t count;
    ecsvm_status_t status;

    if (engine == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }
    if (!engine->pipeline_dirty) {
        return ECSVM_OK;
    }

    order = NULL;
    count = 0u;
    status = ecsvm_pipeline_build(engine->systems, engine->system_count, &order, &count);
    if (status != ECSVM_OK) {
        return status;
    }

    free(engine->system_pipeline);
    engine->system_pipeline = order;
    engine->system_pipeline_count = count;
    engine->pipeline_dirty = 0;
    return ECSVM_OK;
}

static int ecsvm_has_entity(const ecsvm_engine_t *engine, ecsvm_entity_t entity)
{
    size_t index;

    if (engine == NULL || entity == ECSVM_INVALID_ENTITY) {
        return 0;
    }

    for (index = 0u; index < engine->entity_count; ++index) {
        if (engine->entities[index] == entity) {
            return 1;
        }
    }

    return 0;
}

static ptrdiff_t ecsvm_find_component_index_by_name(
    const ecsvm_engine_t *engine,
    const char *name
)
{
    size_t index;

    if (engine == NULL || name == NULL) {
        return -1;
    }

    for (index = 0u; index < engine->component_count; ++index) {
        if (strcmp(engine->components[index].name, name) == 0) {
            return (ptrdiff_t)index;
        }
    }

    return -1;
}

static ptrdiff_t ecsvm_find_system_index_by_name(
    const ecsvm_engine_t *engine,
    const char *name
)
{
    size_t index;

    if (engine == NULL || name == NULL) {
        return -1;
    }

    for (index = 0u; index < engine->system_count; ++index) {
        if (strcmp(engine->systems[index].name, name) == 0) {
            return (ptrdiff_t)index;
        }
    }

    return -1;
}

static ecsvm_component_store_t *ecsvm_component_store_mutable(
    ecsvm_engine_t *engine,
    ecsvm_component_id_t component_id
)
{
    size_t index;

    if (engine == NULL || component_id == ECSVM_INVALID_COMPONENT) {
        return NULL;
    }

    index = (size_t)(component_id - 1u);
    if (index >= engine->component_count) {
        return NULL;
    }

    return &engine->components[index];
}

static const ecsvm_component_store_t *ecsvm_component_store_const(
    const ecsvm_engine_t *engine,
    ecsvm_component_id_t component_id
)
{
    size_t index;

    if (engine == NULL || component_id == ECSVM_INVALID_COMPONENT) {
        return NULL;
    }

    index = (size_t)(component_id - 1u);
    if (index >= engine->component_count) {
        return NULL;
    }

    return &engine->components[index];
}

static ptrdiff_t ecsvm_find_component_entity_index(
    const ecsvm_component_store_t *store,
    ecsvm_entity_t entity
)
{
    size_t index;

    if (store == NULL || entity == ECSVM_INVALID_ENTITY) {
        return -1;
    }

    for (index = 0u; index < store->count; ++index) {
        if (store->entities[index] == entity) {
            return (ptrdiff_t)index;
        }
    }

    return -1;
}

static ecsvm_status_t ecsvm_reserve_entities(ecsvm_engine_t *engine, size_t minimum)
{
    ecsvm_entity_t *entities;
    size_t capacity;

    if (engine == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    if (minimum <= engine->entity_capacity) {
        return ECSVM_OK;
    }

    capacity = ecsvm_next_capacity(engine->entity_capacity, minimum);
    entities = (ecsvm_entity_t *)realloc(engine->entities, capacity * sizeof(*entities));
    if (entities == NULL) {
        return ECSVM_ERROR_MEMORY;
    }

    engine->entities = entities;
    engine->entity_capacity = capacity;
    return ECSVM_OK;
}

static ecsvm_status_t ecsvm_reserve_components(ecsvm_engine_t *engine, size_t minimum)
{
    ecsvm_component_store_t *components;
    size_t capacity;

    if (engine == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    if (minimum <= engine->component_capacity) {
        return ECSVM_OK;
    }

    capacity = ecsvm_next_capacity(engine->component_capacity, minimum);
    components = (ecsvm_component_store_t *)realloc(
        engine->components,
        capacity * sizeof(*components)
    );
    if (components == NULL) {
        return ECSVM_ERROR_MEMORY;
    }

    memset(components + engine->component_capacity, 0, (capacity - engine->component_capacity) * sizeof(*components));
    engine->components = components;
    engine->component_capacity = capacity;
    return ECSVM_OK;
}

static ecsvm_status_t ecsvm_reserve_component_rows(
    ecsvm_component_store_t *store,
    size_t minimum
)
{
    ecsvm_entity_t *entities;
    unsigned char *data;
    size_t capacity;

    if (store == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    if (minimum <= store->capacity) {
        return ECSVM_OK;
    }

    capacity = ecsvm_next_capacity(store->capacity, minimum);
    entities = (ecsvm_entity_t *)realloc(store->entities, capacity * sizeof(*entities));
    if (entities == NULL) {
        return ECSVM_ERROR_MEMORY;
    }

    data = (unsigned char *)realloc(store->data, capacity * store->size);
    if (data == NULL) {
        store->entities = entities;
        return ECSVM_ERROR_MEMORY;
    }

    store->entities = entities;
    store->data = data;
    store->capacity = capacity;
    return ECSVM_OK;
}

static ecsvm_status_t ecsvm_reserve_systems(ecsvm_engine_t *engine, size_t minimum)
{
    ecsvm_system_entry_t *systems;
    size_t capacity;

    if (engine == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    if (minimum <= engine->system_capacity) {
        return ECSVM_OK;
    }

    capacity = ecsvm_next_capacity(engine->system_capacity, minimum);
    systems = (ecsvm_system_entry_t *)realloc(engine->systems, capacity * sizeof(*systems));
    if (systems == NULL) {
        return ECSVM_ERROR_MEMORY;
    }

    memset(systems + engine->system_capacity, 0, (capacity - engine->system_capacity) * sizeof(*systems));
    engine->systems = systems;
    engine->system_capacity = capacity;
    return ECSVM_OK;
}

static ecsvm_status_t ecsvm_reserve_blobs(ecsvm_engine_t *engine, size_t minimum)
{
    ecsvm_blob_slot_t *blobs;
    size_t capacity;

    if (engine == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    if (minimum <= engine->blob_capacity) {
        return ECSVM_OK;
    }

    capacity = ecsvm_next_capacity(engine->blob_capacity, minimum);
    blobs = (ecsvm_blob_slot_t *)realloc(engine->blobs, capacity * sizeof(*blobs));
    if (blobs == NULL) {
        return ECSVM_ERROR_MEMORY;
    }

    memset(blobs + engine->blob_capacity, 0, (capacity - engine->blob_capacity) * sizeof(*blobs));
    engine->blobs = blobs;
    engine->blob_capacity = capacity;
    return ECSVM_OK;
}

static ecsvm_status_t ecsvm_reserve_functions(ecsvm_engine_t *engine, size_t minimum)
{
    ecsvm_function_entry_t *functions;
    size_t capacity;

    if (engine == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    if (minimum <= engine->function_capacity) {
        return ECSVM_OK;
    }

    capacity = ecsvm_next_capacity(engine->function_capacity, minimum);
    functions = (ecsvm_function_entry_t *)realloc(engine->functions, capacity * sizeof(*functions));
    if (functions == NULL) {
        return ECSVM_ERROR_MEMORY;
    }

    memset(functions + engine->function_capacity, 0, (capacity - engine->function_capacity) * sizeof(*functions));
    engine->functions = functions;
    engine->function_capacity = capacity;
    return ECSVM_OK;
}

static void ecsvm_engine_set_error(
    char *error_message,
    size_t error_message_capacity,
    const char *message
)
{
    if (error_message == NULL || error_message_capacity == 0u) {
        return;
    }

    if (message == NULL) {
        error_message[0] = '\0';
        return;
    }

    (void)snprintf(error_message, error_message_capacity, "%s", message);
}

static int ecsvm_parse_function_ast_blob(
    const ecsvm_ecsbin_blob_t *blob,
    ecsvm_ecsbin_ast_blob_t *out_ast,
    char *error_message,
    size_t error_message_capacity
)
{
    uint32_t version;
    uint32_t node_count;
    size_t node_bytes;
    size_t header_bytes;

    if (blob == NULL || out_ast == NULL) {
        ecsvm_engine_set_error(error_message, error_message_capacity, "function body blob is required");
        return 0;
    }

    if (blob->length < sizeof(uint32_t) * 2u) {
        ecsvm_engine_set_error(error_message, error_message_capacity, "function body ast header is truncated");
        return 0;
    }

    memcpy(&version, blob->data, sizeof(version));
    memcpy(&node_count, blob->data + sizeof(uint32_t), sizeof(node_count));
    if (version != ECSVM_ECSBIN_AST_VERSION_3) {
        ecsvm_engine_set_error(error_message, error_message_capacity, "managed runtime only supports ast version 3");
        return 0;
    }

    node_bytes = (size_t)node_count * sizeof(ecsvm_ecsbin_ast_node_t);
    header_bytes = sizeof(uint32_t) * 2u;
    if (node_count == 0u || header_bytes + node_bytes > blob->length) {
        ecsvm_engine_set_error(error_message, error_message_capacity, "function body ast is truncated");
        return 0;
    }

    out_ast->nodes = (const ecsvm_ecsbin_ast_node_t *)(const void *)(blob->data + header_bytes);
    out_ast->node_count = (size_t)node_count;
    out_ast->version = version;
    return 1;
}

static ecsvm_blob_slot_t *ecsvm_blob_slot_mutable(ecsvm_engine_t *engine, ecsvm_blob_t blob)
{
    size_t index;

    if (engine == NULL || blob == ECSVM_INVALID_BLOB) {
        return NULL;
    }

    index = (size_t)(blob - 1u);
    if (index >= engine->blob_count || engine->blobs[index].in_use == 0) {
        return NULL;
    }

    return &engine->blobs[index];
}

static const ecsvm_blob_slot_t *ecsvm_blob_slot_const(
    const ecsvm_engine_t *engine,
    ecsvm_blob_t blob
)
{
    size_t index;

    if (engine == NULL || blob == ECSVM_INVALID_BLOB) {
        return NULL;
    }

    index = (size_t)(blob - 1u);
    if (index >= engine->blob_count || engine->blobs[index].in_use == 0) {
        return NULL;
    }

    return &engine->blobs[index];
}

ecsvm_engine_t *ecsvm_engine_create(void)
{
    ecsvm_engine_t *engine;

    engine = (ecsvm_engine_t *)calloc(1u, sizeof(*engine));
    if (engine == NULL) {
        return NULL;
    }

    engine->next_entity_id = 1u;
    return engine;
}

void ecsvm_engine_destroy(ecsvm_engine_t *engine)
{
    size_t index;

    if (engine == NULL) {
        return;
    }

    for (index = 0u; index < engine->component_count; ++index) {
        free(engine->components[index].name);
        free(engine->components[index].entities);
        free(engine->components[index].data);
    }

    for (index = 0u; index < engine->system_count; ++index) {
        ecsvm_free_system_entry(&engine->systems[index]);
    }

    for (index = 0u; index < engine->blob_count; ++index) {
        free(engine->blobs[index].data);
    }

    ecsvm_engine_clear_functions(engine);
    free(engine->entities);
    free(engine->components);
    free(engine->systems);
    free(engine->system_pipeline);
    free(engine->blobs);
    free(engine);
}

void ecsvm_engine_clear_functions(ecsvm_engine_t *engine)
{
    if (engine == NULL) {
        return;
    }

    free(engine->functions);
    engine->functions = NULL;
    engine->function_count = 0u;
    engine->function_capacity = 0u;
}

ecsvm_status_t ecsvm_engine_load_functions(
    ecsvm_engine_t *engine,
    const ecsvm_ecsbin_module_t *module,
    char *error_message,
    size_t error_message_capacity
)
{
    ecsvm_status_t status;
    size_t index;

    ecsvm_engine_set_error(error_message, error_message_capacity, NULL);
    if (engine == NULL || module == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    ecsvm_engine_clear_functions(engine);
    if (module->function_ref_count == 0u) {
        return ECSVM_OK;
    }

    status = ecsvm_reserve_functions(engine, module->function_ref_count);
    if (status != ECSVM_OK) {
        ecsvm_engine_set_error(error_message, error_message_capacity, "out of memory while preparing function table");
        return status;
    }

    for (index = 0u; index < module->function_ref_count; ++index) {
        const ecsvm_ecsbin_function_ref_t *function_ref;
        ecsvm_function_entry_t *entry;

        function_ref = &module->function_refs[index];
        entry = &engine->functions[index];
        memset(entry, 0, sizeof(*entry));
        entry->function_ref = function_ref;
        entry->native_callback = ecsvm_core_find_native_function(function_ref->qualified_name);

        if (function_ref->body_blob_id != 0u) {
            const ecsvm_ecsbin_blob_t *blob;

            blob = ecsvm_ecsbin_blob_ref(module, function_ref->body_blob_id);
            if (!ecsvm_parse_function_ast_blob(
                    blob,
                    &entry->managed_body,
                    error_message,
                    error_message_capacity
                )) {
                ecsvm_engine_clear_functions(engine);
                return ECSVM_ERROR_ARGUMENT;
            }
            entry->has_managed_body = 1;
        }
    }

    engine->function_count = module->function_ref_count;
    return ECSVM_OK;
}

const ecsvm_function_entry_t *ecsvm_engine_function(
    const ecsvm_engine_t *engine,
    uint32_t function_id
)
{
    if (engine == NULL || function_id == 0u || function_id > engine->function_count) {
        return NULL;
    }

    return &engine->functions[function_id - 1u];
}

ecsvm_status_t ecsvm_engine_register_builtin_components(ecsvm_engine_t *engine)
{
    ecsvm_component_desc_t desc;
    ecsvm_component_id_t component_id;
    ptrdiff_t existing;

    if (engine == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    if (engine->hierarchy_component != ECSVM_INVALID_COMPONENT) {
        return ECSVM_OK;
    }

    existing = ecsvm_find_component_index_by_name(engine, "core.Hierarchy");
    if (existing >= 0) {
        engine->hierarchy_component = (ecsvm_component_id_t)((size_t)existing + 1u);
        return ECSVM_OK;
    }

    desc.name = "core.Hierarchy";
    desc.size = sizeof(ecsvm_hierarchy_component_t);
    desc.preferred_storage = ECSVM_STORAGE_CONTIGUOUS;

    if (ecsvm_engine_register_component(engine, &desc, &component_id) != ECSVM_OK) {
        return ECSVM_ERROR_MEMORY;
    }

    engine->hierarchy_component = component_id;
    return ECSVM_OK;
}

ecsvm_component_id_t ecsvm_engine_hierarchy_component(const ecsvm_engine_t *engine)
{
    if (engine == NULL) {
        return ECSVM_INVALID_COMPONENT;
    }

    return engine->hierarchy_component;
}

ecsvm_component_id_t ecsvm_engine_find_component(
    const ecsvm_engine_t *engine,
    const char *name
)
{
    ptrdiff_t index;

    index = ecsvm_find_component_index_by_name(engine, name);
    if (index < 0) {
        return ECSVM_INVALID_COMPONENT;
    }

    return (ecsvm_component_id_t)((size_t)index + 1u);
}

ecsvm_status_t ecsvm_engine_register_component(
    ecsvm_engine_t *engine,
    const ecsvm_component_desc_t *desc,
    ecsvm_component_id_t *out_component
)
{
    ecsvm_component_store_t *store;
    char *name;
    ecsvm_status_t status;

    if (engine == NULL || desc == NULL || desc->name == NULL || desc->name[0] == '\0' || desc->size == 0u) {
        return ECSVM_ERROR_ARGUMENT;
    }

    if (ecsvm_find_component_index_by_name(engine, desc->name) >= 0) {
        return ECSVM_ERROR_EXISTS;
    }

    status = ecsvm_reserve_components(engine, engine->component_count + 1u);
    if (status != ECSVM_OK) {
        return status;
    }

    name = ecsvm_copy_string(desc->name);
    if (name == NULL) {
        return ECSVM_ERROR_MEMORY;
    }

    store = &engine->components[engine->component_count];
    memset(store, 0, sizeof(*store));
    store->name = name;
    store->size = desc->size;
    store->storage_mode = desc->preferred_storage;
    engine->component_count += 1u;

    if (out_component != NULL) {
        *out_component = (ecsvm_component_id_t)engine->component_count;
    }

    return ECSVM_OK;
}

ecsvm_status_t ecsvm_engine_register_system(
    ecsvm_engine_t *engine,
    const ecsvm_system_desc_t *desc,
    size_t *out_system_index
)
{
    ecsvm_system_entry_t *entry;
    char *name;
    ecsvm_status_t status;

    if (engine == NULL ||
        desc == NULL ||
        desc->name == NULL ||
        desc->name[0] == '\0' ||
        desc->callback == NULL ||
        (desc->before_count > 0u && desc->before == NULL) ||
        (desc->after_count > 0u && desc->after == NULL)) {
        return ECSVM_ERROR_ARGUMENT;
    }

    if (ecsvm_find_system_index_by_name(engine, desc->name) >= 0) {
        return ECSVM_ERROR_EXISTS;
    }

    status = ecsvm_reserve_systems(engine, engine->system_count + 1u);
    if (status != ECSVM_OK) {
        return status;
    }

    name = ecsvm_copy_string(desc->name);
    if (name == NULL) {
        return ECSVM_ERROR_MEMORY;
    }

    entry = &engine->systems[engine->system_count];
    memset(entry, 0, sizeof(*entry));
    entry->name = name;
    entry->callback = desc->callback;
    entry->api.alloc = desc->alloc != NULL ? desc->alloc : ecsvm_default_alloc;
    entry->api.free = desc->free != NULL ? desc->free : ecsvm_default_free;
    entry->api.log = desc->log != NULL ? desc->log : ecsvm_default_log;
    entry->api.userdata = desc->user_data;
    entry->before = ecsvm_copy_string_array(desc->before, desc->before_count);
    entry->before_count = desc->before_count;
    entry->after = ecsvm_copy_string_array(desc->after, desc->after_count);
    entry->after_count = desc->after_count;
    if ((desc->before_count > 0u && entry->before == NULL) ||
        (desc->after_count > 0u && entry->after == NULL)) {
        ecsvm_free_string_array(entry->before, entry->before_count);
        ecsvm_free_string_array(entry->after, entry->after_count);
        free(entry->name);
        memset(entry, 0, sizeof(*entry));
        return ECSVM_ERROR_MEMORY;
    }
    engine->system_count += 1u;
    engine->pipeline_dirty = 1;

    if (out_system_index != NULL) {
        *out_system_index = engine->system_count - 1u;
    }

    return ECSVM_OK;
}

ecsvm_status_t ecsvm_engine_unregister_system(ecsvm_engine_t *engine, const char *name)
{
    ptrdiff_t index;
    size_t remove_index;

    if (engine == NULL || name == NULL || name[0] == '\0') {
        return ECSVM_ERROR_ARGUMENT;
    }

    index = ecsvm_find_system_index_by_name(engine, name);
    if (index < 0) {
        return ECSVM_ERROR_NOT_FOUND;
    }

    remove_index = (size_t)index;
    ecsvm_free_system_entry(&engine->systems[remove_index]);
    if (remove_index + 1u < engine->system_count) {
        memmove(
            &engine->systems[remove_index],
            &engine->systems[remove_index + 1u],
            (engine->system_count - remove_index - 1u) * sizeof(*engine->systems)
        );
    }

    engine->system_count -= 1u;
    memset(&engine->systems[engine->system_count], 0, sizeof(*engine->systems));
    engine->pipeline_dirty = 1;
    return ECSVM_OK;
}

ecsvm_status_t ecsvm_engine_tick(ecsvm_engine_t *engine)
{
    ecsvm_context_t context;
    ecsvm_status_t status;
    size_t index;

    if (engine == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    memset(&context, 0, sizeof(context));
    context.engine = engine;

    status = ecsvm_engine_ensure_pipeline(engine);
    if (status != ECSVM_OK) {
        return status;
    }

    for (index = 0u; index < engine->system_pipeline_count; ++index) {
        size_t system_index;

        system_index = engine->system_pipeline[index];
        context.system_index = index;
        context.system_name = engine->systems[system_index].name;
        context.api = engine->systems[system_index].api;

        status = engine->systems[system_index].callback(&context);
        if (status != ECSVM_OK) {
            return status == ECSVM_OK ? ECSVM_ERROR_CALLBACK : status;
        }
    }

    return ECSVM_OK;
}

ecsvm_status_t ecsvm_engine_run(ecsvm_engine_t *engine)
{
    ecsvm_status_t status;

    if (engine == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    engine->stop_requested = 0;
    do {
        status = ecsvm_engine_tick(engine);
        if (status != ECSVM_OK) {
            return status;
        }
    } while (!engine->stop_requested);

    return ECSVM_OK;
}

void ecsvm_engine_request_stop(ecsvm_engine_t *engine)
{
    if (engine != NULL) {
        engine->stop_requested = 1;
    }
}

void ecsvm_engine_clear_stop(ecsvm_engine_t *engine)
{
    if (engine != NULL) {
        engine->stop_requested = 0;
    }
}

int ecsvm_engine_stop_requested(const ecsvm_engine_t *engine)
{
    return engine != NULL && engine->stop_requested != 0;
}

size_t ecsvm_engine_system_count(const ecsvm_engine_t *engine)
{
    if (engine == NULL) {
        return 0u;
    }

    return engine->system_count;
}

ecsvm_entity_t ecsvm_entity_create(ecsvm_engine_t *engine)
{
    ecsvm_status_t status;
    ecsvm_entity_t entity;
    ecsvm_hierarchy_component_t hierarchy;

    if (engine == NULL) {
        return ECSVM_INVALID_ENTITY;
    }

    status = ecsvm_engine_register_builtin_components(engine);
    if (status != ECSVM_OK) {
        return ECSVM_INVALID_ENTITY;
    }

    status = ecsvm_reserve_entities(engine, engine->entity_count + 1u);
    if (status != ECSVM_OK) {
        return ECSVM_INVALID_ENTITY;
    }

    entity = engine->next_entity_id;
    engine->next_entity_id += 1u;
    engine->entities[engine->entity_count] = entity;
    engine->entity_count += 1u;

    hierarchy.parent = ECSVM_INVALID_ENTITY;
    hierarchy.first_child = ECSVM_INVALID_ENTITY;
    hierarchy.next_sibling = ECSVM_INVALID_ENTITY;
    status = ecsvm_component_set(engine, engine->hierarchy_component, entity, &hierarchy);
    if (status != ECSVM_OK) {
        engine->entity_count -= 1u;
        engine->next_entity_id -= 1u;
        return ECSVM_INVALID_ENTITY;
    }

    return entity;
}

size_t ecsvm_entity_count(const ecsvm_engine_t *engine)
{
    if (engine == NULL) {
        return 0u;
    }

    return engine->entity_count;
}

ecsvm_entity_t ecsvm_entity_at(const ecsvm_engine_t *engine, size_t index)
{
    if (engine == NULL || index >= engine->entity_count) {
        return ECSVM_INVALID_ENTITY;
    }

    return engine->entities[index];
}

ecsvm_status_t ecsvm_component_set(
    ecsvm_engine_t *engine,
    ecsvm_component_id_t component_id,
    ecsvm_entity_t entity,
    const void *data
)
{
    ecsvm_component_store_t *store;
    ecsvm_status_t status;
    ptrdiff_t existing;
    unsigned char *destination;

    if (engine == NULL || data == NULL || !ecsvm_has_entity(engine, entity)) {
        return ECSVM_ERROR_ARGUMENT;
    }

    store = ecsvm_component_store_mutable(engine, component_id);
    if (store == NULL) {
        return ECSVM_ERROR_NOT_FOUND;
    }

    existing = ecsvm_find_component_entity_index(store, entity);
    if (existing >= 0) {
        destination = store->data + ((size_t)existing * store->size);
        memcpy(destination, data, store->size);
        return ECSVM_OK;
    }

    status = ecsvm_reserve_component_rows(store, store->count + 1u);
    if (status != ECSVM_OK) {
        return status;
    }

    store->entities[store->count] = entity;
    destination = store->data + (store->count * store->size);
    memcpy(destination, data, store->size);
    store->count += 1u;
    return ECSVM_OK;
}

void *ecsvm_component_get_mutable(
    ecsvm_engine_t *engine,
    ecsvm_component_id_t component_id,
    ecsvm_entity_t entity
)
{
    ecsvm_component_store_t *store;
    ptrdiff_t index;

    store = ecsvm_component_store_mutable(engine, component_id);
    if (store == NULL) {
        return NULL;
    }

    index = ecsvm_find_component_entity_index(store, entity);
    if (index < 0) {
        return NULL;
    }

    return store->data + ((size_t)index * store->size);
}

const void *ecsvm_component_get(
    const ecsvm_engine_t *engine,
    ecsvm_component_id_t component_id,
    ecsvm_entity_t entity
)
{
    const ecsvm_component_store_t *store;
    ptrdiff_t index;

    store = ecsvm_component_store_const(engine, component_id);
    if (store == NULL) {
        return NULL;
    }

    index = ecsvm_find_component_entity_index(store, entity);
    if (index < 0) {
        return NULL;
    }

    return store->data + ((size_t)index * store->size);
}

int ecsvm_component_has(
    const ecsvm_engine_t *engine,
    ecsvm_component_id_t component_id,
    ecsvm_entity_t entity
)
{
    return ecsvm_component_get(engine, component_id, entity) != NULL;
}

size_t ecsvm_component_count(const ecsvm_engine_t *engine, ecsvm_component_id_t component_id)
{
    const ecsvm_component_store_t *store;

    store = ecsvm_component_store_const(engine, component_id);
    if (store == NULL) {
        return 0u;
    }

    return store->count;
}

ecsvm_status_t ecsvm_blob_create(ecsvm_engine_t *engine, size_t size, ecsvm_blob_t *out_blob)
{
    ecsvm_blob_slot_t *slot;
    unsigned char *data;
    size_t index;
    ecsvm_status_t status;

    if (engine == NULL || out_blob == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    for (index = 0u; index < engine->blob_count; ++index) {
        if (engine->blobs[index].in_use == 0) {
            slot = &engine->blobs[index];
            data = (unsigned char *)malloc(size == 0u ? 1u : size);
            if (data == NULL) {
                return ECSVM_ERROR_MEMORY;
            }

            slot->data = data;
            slot->size = size;
            slot->in_use = 1;
            slot->is_string = 0;
            *out_blob = (ecsvm_blob_t)(index + 1u);
            return ECSVM_OK;
        }
    }

    status = ecsvm_reserve_blobs(engine, engine->blob_count + 1u);
    if (status != ECSVM_OK) {
        return status;
    }

    slot = &engine->blobs[engine->blob_count];
    data = (unsigned char *)malloc(size == 0u ? 1u : size);
    if (data == NULL) {
        return ECSVM_ERROR_MEMORY;
    }

    memset(slot, 0, sizeof(*slot));
    slot->data = data;
    slot->size = size;
    slot->in_use = 1;
    engine->blob_count += 1u;
    *out_blob = (ecsvm_blob_t)engine->blob_count;
    return ECSVM_OK;
}

ecsvm_status_t ecsvm_blob_write(
    ecsvm_engine_t *engine,
    ecsvm_blob_t blob,
    size_t offset,
    const void *data,
    size_t size
)
{
    ecsvm_blob_slot_t *slot;

    if (engine == NULL || data == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    slot = ecsvm_blob_slot_mutable(engine, blob);
    if (slot == NULL) {
        return ECSVM_ERROR_NOT_FOUND;
    }

    if (offset > slot->size || size > slot->size - offset) {
        return ECSVM_ERROR_ARGUMENT;
    }

    memcpy(slot->data + offset, data, size);
    return ECSVM_OK;
}

void *ecsvm_blob_data(ecsvm_engine_t *engine, ecsvm_blob_t blob)
{
    ecsvm_blob_slot_t *slot;

    slot = ecsvm_blob_slot_mutable(engine, blob);
    if (slot == NULL) {
        return NULL;
    }

    return slot->data;
}

const void *ecsvm_blob_data_const(const ecsvm_engine_t *engine, ecsvm_blob_t blob)
{
    const ecsvm_blob_slot_t *slot;

    slot = ecsvm_blob_slot_const(engine, blob);
    if (slot == NULL) {
        return NULL;
    }

    return slot->data;
}

size_t ecsvm_blob_size(const ecsvm_engine_t *engine, ecsvm_blob_t blob)
{
    const ecsvm_blob_slot_t *slot;

    slot = ecsvm_blob_slot_const(engine, blob);
    if (slot == NULL) {
        return 0u;
    }

    return slot->size;
}

int ecsvm_blob_is_string(const ecsvm_engine_t *engine, ecsvm_blob_t blob)
{
    const ecsvm_blob_slot_t *slot;

    slot = ecsvm_blob_slot_const(engine, blob);
    if (slot == NULL) {
        return 0;
    }

    return slot->is_string;
}

ecsvm_status_t ecsvm_blob_destroy(ecsvm_engine_t *engine, ecsvm_blob_t blob)
{
    ecsvm_blob_slot_t *slot;

    slot = ecsvm_blob_slot_mutable(engine, blob);
    if (slot == NULL) {
        return ECSVM_ERROR_NOT_FOUND;
    }

    free(slot->data);
    memset(slot, 0, sizeof(*slot));
    return ECSVM_OK;
}

ecsvm_status_t ecsvm_string_create(
    ecsvm_engine_t *engine,
    const char *text,
    ecsvm_blob_t *out_blob
)
{
    ecsvm_status_t status;
    ecsvm_blob_slot_t *slot;
    ecsvm_blob_t blob;
    size_t length;

    if (engine == NULL || text == NULL || out_blob == NULL) {
        return ECSVM_ERROR_ARGUMENT;
    }

    length = strlen(text) + 1u;
    status = ecsvm_blob_create(engine, length, &blob);
    if (status != ECSVM_OK) {
        return status;
    }

    status = ecsvm_blob_write(engine, blob, 0u, text, length);
    if (status != ECSVM_OK) {
        ecsvm_blob_destroy(engine, blob);
        return status;
    }

    slot = ecsvm_blob_slot_mutable(engine, blob);
    if (slot == NULL) {
        ecsvm_blob_destroy(engine, blob);
        return ECSVM_ERROR_NOT_FOUND;
    }

    slot->is_string = 1;
    *out_blob = blob;
    return ECSVM_OK;
}

const char *ecsvm_string_cstr(const ecsvm_engine_t *engine, ecsvm_blob_t blob)
{
    const ecsvm_blob_slot_t *slot;

    slot = ecsvm_blob_slot_const(engine, blob);
    if (slot == NULL || slot->is_string == 0) {
        return NULL;
    }

    return (const char *)slot->data;
}

const char *ecsvm_status_string(ecsvm_status_t status)
{
    switch (status) {
    case ECSVM_OK:
        return "ok";
    case ECSVM_ERROR_ARGUMENT:
        return "argument-error";
    case ECSVM_ERROR_MEMORY:
        return "memory-error";
    case ECSVM_ERROR_NOT_FOUND:
        return "not-found";
    case ECSVM_ERROR_EXISTS:
        return "exists";
    case ECSVM_ERROR_CALLBACK:
        return "callback-error";
    default:
        return "unknown-status";
    }
}
