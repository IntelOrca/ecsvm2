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

typedef struct ecsvm_input_monitor_component {
    int32_t kind;
    int32_t device;
    int32_t index;
    float value;
} ecsvm_input_monitor_component_t;

typedef struct ecsvm_time_component {
    uint64_t tick_count;
    uint64_t last_tick_duration_ns;
} ecsvm_time_component_t;

typedef struct ecsvm_window_component {
    int32_t width;
    int32_t height;
    float delta_seconds;
    unsigned char closing;
} ecsvm_window_component_t;

typedef struct ecsvm_core_components {
    ecsvm_component_id_t hierarchy;
    ecsvm_component_id_t transform;
    ecsvm_component_id_t time;
    ecsvm_component_id_t graphics_shape;
    ecsvm_component_id_t input_monitor;
    ecsvm_component_id_t window;
} ecsvm_core_components_t;

typedef enum ecsvm_shape_kind {
    ECSVM_SHAPE_RECTANGLE = 1,
    ECSVM_SHAPE_CIRCLE = 2
} ecsvm_shape_kind_t;

typedef enum ecsvm_input_monitor_kind {
    ECSVM_INPUT_MONITOR_BUTTON = 1,
    ECSVM_INPUT_MONITOR_AXIS = 2
} ecsvm_input_monitor_kind_t;

typedef enum ecsvm_input_monitor_device {
    ECSVM_INPUT_DEVICE_KEYBOARD = 1,
    ECSVM_INPUT_DEVICE_MOUSE = 2
} ecsvm_input_monitor_device_t;

typedef enum ecsvm_input_monitor_index {
    ECSVM_INPUT_MOUSE_X = 0,
    ECSVM_INPUT_MOUSE_Y = 1,
    ECSVM_INPUT_MOUSE_BUTTON_LEFT = 1
} ecsvm_input_monitor_index_t;

ecsvm_status_t ecsvm_register_core_components(
    ecsvm_engine_t *engine,
    ecsvm_core_components_t *out_components
);

#ifdef __cplusplus
}
#endif

#endif
