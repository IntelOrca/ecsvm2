#ifndef ECSVM_ECSVM_H
#define ECSVM_ECSVM_H

#include <stddef.h>
#include <stdint.h>

#ifndef ECSVM_ENABLE_SDL3
#define ECSVM_ENABLE_SDL3 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t ecsvm_entity_t;
typedef uint32_t ecsvm_component_id_t;
typedef uint32_t ecsvm_blob_t;

enum {
    ECSVM_INVALID_ENTITY = 0u,
    ECSVM_INVALID_COMPONENT = 0u,
    ECSVM_INVALID_BLOB = 0u
};

typedef enum ecsvm_status {
    ECSVM_OK = 0,
    ECSVM_ERROR_ARGUMENT,
    ECSVM_ERROR_MEMORY,
    ECSVM_ERROR_NOT_FOUND,
    ECSVM_ERROR_EXISTS,
    ECSVM_ERROR_CALLBACK
} ecsvm_status_t;

typedef enum ecsvm_storage_mode {
    ECSVM_STORAGE_CONTIGUOUS = 0
} ecsvm_storage_mode_t;

typedef struct ecsvm_engine ecsvm_engine_t;
typedef struct ecsvm_context ecsvm_context_t;

typedef void *(*ecsvm_alloc_fn)(void *userdata, size_t size);
typedef void (*ecsvm_free_fn)(void *userdata, void *ptr);
typedef void (*ecsvm_log_fn)(void *userdata, const char *message);
typedef ecsvm_status_t (*ecsvm_system_fn)(ecsvm_context_t *ctx);

typedef struct ecsvm_component_desc {
    const char *name;
    size_t size;
    ecsvm_storage_mode_t preferred_storage;
} ecsvm_component_desc_t;

typedef struct ecsvm_system_api {
    ecsvm_alloc_fn alloc;
    ecsvm_free_fn free;
    ecsvm_log_fn log;
    void *userdata;
} ecsvm_system_api_t;

typedef struct ecsvm_system_desc {
    const char *name;
    ecsvm_system_fn callback;
    ecsvm_alloc_fn alloc;
    ecsvm_free_fn free;
    ecsvm_log_fn log;
    void *user_data;
    const char *const *before;
    size_t before_count;
    const char *const *after;
    size_t after_count;
} ecsvm_system_desc_t;

typedef struct ecsvm_hierarchy_component {
    ecsvm_entity_t parent;
    ecsvm_entity_t first_child;
    ecsvm_entity_t next_sibling;
} ecsvm_hierarchy_component_t;

struct ecsvm_context {
    ecsvm_engine_t *engine;
    size_t system_index;
    const char *system_name;
    ecsvm_system_api_t api;
};

ecsvm_engine_t *ecsvm_engine_create(void);
void ecsvm_engine_destroy(ecsvm_engine_t *engine);

ecsvm_status_t ecsvm_engine_register_builtin_components(ecsvm_engine_t *engine);
ecsvm_component_id_t ecsvm_engine_hierarchy_component(const ecsvm_engine_t *engine);
ecsvm_component_id_t ecsvm_engine_find_component(
    const ecsvm_engine_t *engine,
    const char *name
);

ecsvm_status_t ecsvm_engine_register_component(
    ecsvm_engine_t *engine,
    const ecsvm_component_desc_t *desc,
    ecsvm_component_id_t *out_component
);

ecsvm_status_t ecsvm_engine_register_system(
    ecsvm_engine_t *engine,
    const ecsvm_system_desc_t *desc,
    size_t *out_system_index
);

ecsvm_status_t ecsvm_engine_tick(ecsvm_engine_t *engine);
ecsvm_status_t ecsvm_engine_run(ecsvm_engine_t *engine);
void ecsvm_engine_request_stop(ecsvm_engine_t *engine);
void ecsvm_engine_clear_stop(ecsvm_engine_t *engine);
int ecsvm_engine_stop_requested(const ecsvm_engine_t *engine);
size_t ecsvm_engine_system_count(const ecsvm_engine_t *engine);

ecsvm_entity_t ecsvm_entity_create(ecsvm_engine_t *engine);
size_t ecsvm_entity_count(const ecsvm_engine_t *engine);
ecsvm_entity_t ecsvm_entity_at(const ecsvm_engine_t *engine, size_t index);

ecsvm_status_t ecsvm_component_set(
    ecsvm_engine_t *engine,
    ecsvm_component_id_t component_id,
    ecsvm_entity_t entity,
    const void *data
);

void *ecsvm_component_get_mutable(
    ecsvm_engine_t *engine,
    ecsvm_component_id_t component_id,
    ecsvm_entity_t entity
);

const void *ecsvm_component_get(
    const ecsvm_engine_t *engine,
    ecsvm_component_id_t component_id,
    ecsvm_entity_t entity
);

int ecsvm_component_has(
    const ecsvm_engine_t *engine,
    ecsvm_component_id_t component_id,
    ecsvm_entity_t entity
);

size_t ecsvm_component_count(const ecsvm_engine_t *engine, ecsvm_component_id_t component_id);

ecsvm_status_t ecsvm_blob_create(ecsvm_engine_t *engine, size_t size, ecsvm_blob_t *out_blob);
ecsvm_status_t ecsvm_blob_write(
    ecsvm_engine_t *engine,
    ecsvm_blob_t blob,
    size_t offset,
    const void *data,
    size_t size
);

void *ecsvm_blob_data(ecsvm_engine_t *engine, ecsvm_blob_t blob);
const void *ecsvm_blob_data_const(const ecsvm_engine_t *engine, ecsvm_blob_t blob);
size_t ecsvm_blob_size(const ecsvm_engine_t *engine, ecsvm_blob_t blob);
int ecsvm_blob_is_string(const ecsvm_engine_t *engine, ecsvm_blob_t blob);
ecsvm_status_t ecsvm_blob_destroy(ecsvm_engine_t *engine, ecsvm_blob_t blob);

ecsvm_status_t ecsvm_string_create(
    ecsvm_engine_t *engine,
    const char *text,
    ecsvm_blob_t *out_blob
);

const char *ecsvm_string_cstr(const ecsvm_engine_t *engine, ecsvm_blob_t blob);
const char *ecsvm_status_string(ecsvm_status_t status);

#ifdef __cplusplus
}
#endif

#endif
