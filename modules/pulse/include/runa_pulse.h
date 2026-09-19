#ifndef RUNA_PULSE_H
#define RUNA_PULSE_H

#include "runa_module.h"

#define RUNA_PULSE_MODULE_ID 8u
#define RUNA_PULSE_RESOURCE_TYPE 1u
#define RUNA_PULSE_OP_COUNT 1u
#define RUNA_PULSE_OP_MEASURE_WIDTH 2u
#define RUNA_PULSE_OP_MEASURE_PERIOD 3u

#define RUNA_PULSE_MAX_WINDOW_US 5000000u
#define RUNA_PULSE_MAX_TIMEOUT_US 5000000u
#define RUNA_PULSE_MAX_COUNT 1000000u

enum {
    RUNA_PULSE_EDGE_RISING = 0u,
    RUNA_PULSE_EDGE_FALLING = 1u,
    RUNA_PULSE_EDGE_BOTH = 2u
};

enum {
    RUNA_PULSE_LEVEL_HIGH = 1u,
    RUNA_PULSE_LEVEL_LOW = 2u
};

enum {
    RUNA_PULSE_ERR_IO = RUNA_ERR_MODULE_BASE + 32u,
    RUNA_PULSE_ERR_INVALID_MEASUREMENT = RUNA_ERR_MODULE_BASE + 33u
};

typedef struct runa_pulse_resource_config {
    uint32_t minimum_window_us;
    uint32_t maximum_window_us;
    uint32_t minimum_timeout_us;
    uint32_t maximum_timeout_us;
    uint32_t maximum_count;
    uint8_t supported_edges;
    uint8_t supported_levels;
    uint16_t reserved;
} runa_pulse_resource_config_t;

typedef struct runa_pulse_hal {
    void *context;
    runa_status_t (*count)(void *context, uintptr_t handle, uint8_t edge,
                           uint32_t window_us, uint32_t max_count, uint32_t *count);
    runa_status_t (*measure_width)(void *context, uintptr_t handle, uint8_t level,
                                   uint32_t timeout_us, uint32_t *width_us);
    runa_status_t (*measure_period)(void *context, uintptr_t handle, uint8_t reference_edge,
                                    uint32_t timeout_us, uint32_t *period_us);
} runa_pulse_hal_t;

runa_module_t runa_pulse_module(runa_pulse_hal_t *hal);

#endif
