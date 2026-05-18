#ifndef ECSVM_SYSTEM_TIME_H
#define ECSVM_SYSTEM_TIME_H

#include "ecsvm/component.h"
#include "ecsvm/ecsvm.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ecsvm_time_system {
    ecsvm_component_id_t time_component;
    ecsvm_entity_t time_entity;
    uint64_t previous_tick_ns;
    int has_previous_tick;
} ecsvm_time_system_t;

void ecsvm_time_system_init(
    ecsvm_time_system_t *system,
    ecsvm_component_id_t time_component
);

ecsvm_status_t ecsvm_time_system_register(
    ecsvm_engine_t *engine,
    ecsvm_time_system_t *system
);

#ifdef __cplusplus
}
#endif

#endif
