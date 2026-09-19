#include "runa_pulse_mock_hal.h"

static runa_status_t count(void *context, uintptr_t handle, uint8_t edge,
                           uint32_t window_us, uint32_t max_count, uint32_t *value) {
    runa_pulse_mock_state_t *state = (runa_pulse_mock_state_t *)context;
    if (state == NULL || value == NULL) return RUNA_ERR_INTERNAL;
    ++state->count_calls;
    state->last_handle = handle;
    state->last_edge = edge;
    state->last_bound = window_us;
    state->last_max_count = max_count;
    *value = state->count_value;
    return state->count_status;
}

static runa_status_t measure_width(void *context, uintptr_t handle, uint8_t level,
                                   uint32_t timeout_us, uint32_t *value) {
    runa_pulse_mock_state_t *state = (runa_pulse_mock_state_t *)context;
    if (state == NULL || value == NULL) return RUNA_ERR_INTERNAL;
    ++state->width_calls;
    state->last_handle = handle;
    state->last_level = level;
    state->last_bound = timeout_us;
    *value = state->width_value;
    return state->width_status;
}

static runa_status_t measure_period(void *context, uintptr_t handle, uint8_t edge,
                                    uint32_t timeout_us, uint32_t *value) {
    runa_pulse_mock_state_t *state = (runa_pulse_mock_state_t *)context;
    if (state == NULL || value == NULL) return RUNA_ERR_INTERNAL;
    ++state->period_calls;
    state->last_handle = handle;
    state->last_edge = edge;
    state->last_bound = timeout_us;
    *value = state->period_value;
    return state->period_status;
}

runa_pulse_hal_t runa_pulse_mock_hal(runa_pulse_mock_state_t *state) {
    runa_pulse_hal_t hal = { state, count, measure_width, measure_period };
    return hal;
}
