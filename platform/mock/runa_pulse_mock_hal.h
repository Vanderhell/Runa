#ifndef RUNA_PULSE_MOCK_HAL_H
#define RUNA_PULSE_MOCK_HAL_H

#include "runa_pulse.h"

typedef struct runa_pulse_mock_state {
    runa_status_t count_status;
    runa_status_t width_status;
    runa_status_t period_status;
    uint32_t count_value;
    uint32_t width_value;
    uint32_t period_value;
    uint32_t count_calls;
    uint32_t width_calls;
    uint32_t period_calls;
    uintptr_t last_handle;
    uint8_t last_edge;
    uint8_t last_level;
    uint32_t last_bound;
    uint32_t last_max_count;
} runa_pulse_mock_state_t;

runa_pulse_hal_t runa_pulse_mock_hal(runa_pulse_mock_state_t *state);

#endif
