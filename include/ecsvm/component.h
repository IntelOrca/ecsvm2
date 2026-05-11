#ifndef ECSVM_COMPONENT_H
#define ECSVM_COMPONENT_H

#include "ecsvm/ecsvm.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ecsvm_vec2 {
    float x;
    float y;
} ecsvm_vec2_t;

typedef struct ecsvm_vec4 {
    float x;
    float y;
    float z;
    float w;
} ecsvm_vec4_t;

typedef struct ecsvm_transform_component {
    ecsvm_vec2_t position;
    ecsvm_vec2_t scale;
    float rotation;
} ecsvm_transform_component_t;

typedef struct ecsvm_graphics_shape_component {
    ecsvm_vec4_t color;
    int32_t kind;
} ecsvm_graphics_shape_component_t;

typedef struct ecsvm_core_components {
    ecsvm_component_id_t hierarchy;
    ecsvm_component_id_t transform;
    ecsvm_component_id_t graphics_shape;
} ecsvm_core_components_t;

typedef enum ecsvm_shape_kind {
    ECSVM_SHAPE_RECTANGLE = 1
} ecsvm_shape_kind_t;

ecsvm_status_t ecsvm_register_core_components(
    ecsvm_engine_t *engine,
    ecsvm_core_components_t *out_components
);

#ifdef __cplusplus
}
#endif

#endif
