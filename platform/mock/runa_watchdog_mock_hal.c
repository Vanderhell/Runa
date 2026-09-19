#include "runa_watchdog_mock_hal.h"

#include <string.h>

static runa_status_t take_status(runa_watchdog_mock_state_t *state) {
    runa_status_t status = state->next_status;
    state->next_status = RUNA_OK;
    return status;
}

static runa_status_t status(void *context, uintptr_t handle, runa_watchdog_status_t *output) {
    runa_watchdog_mock_state_t *state = (runa_watchdog_mock_state_t *)context;
    runa_status_t result;
    (void)handle;
    ++state->status_calls;
    result = take_status(state);
    if (result != RUNA_OK) return result;
    output->flags = state->armed != 0u ? RUNA_WATCHDOG_STATUS_ARMED : 0u;
    if (state->last_reset_watchdog != 0u)
        output->flags |= RUNA_WATCHDOG_STATUS_LAST_RESET_WATCHDOG;
    output->reserved[0] = 0u;
    output->reserved[1] = 0u;
    output->reserved[2] = 0u;
    output->timeout_ms = state->timeout_ms;
    return RUNA_OK;
}

static runa_status_t arm(void *context, uintptr_t handle, uint32_t timeout_ms) {
    runa_watchdog_mock_state_t *state = (runa_watchdog_mock_state_t *)context;
    runa_status_t result;
    (void)handle;
    ++state->arm_calls;
    result = take_status(state);
    if (result == RUNA_OK) {
        state->armed = 1u;
        state->timeout_ms = timeout_ms;
    }
    return result;
}

static runa_status_t feed(void *context, uintptr_t handle) {
    runa_watchdog_mock_state_t *state = (runa_watchdog_mock_state_t *)context;
    runa_status_t result;
    (void)handle;
    ++state->feed_calls;
    result = take_status(state);
    if (result == RUNA_OK) {
        ++state->feed_count;
        state->last_reset_watchdog = 0u;
    }
    return result;
}

static runa_status_t disarm(void *context, uintptr_t handle) {
    runa_watchdog_mock_state_t *state = (runa_watchdog_mock_state_t *)context;
    runa_status_t result;
    (void)handle;
    ++state->disarm_calls;
    if (state->disarm_supported == 0u) return RUNA_WATCHDOG_ERR_UNSUPPORTED;
    result = take_status(state);
    if (result == RUNA_OK) state->armed = 0u;
    return result;
}

void runa_watchdog_mock_init(runa_watchdog_mock_state_t *state) {
    if (state != NULL) {
        memset(state, 0, sizeof *state);
        state->disarm_supported = 1u;
    }
}

runa_watchdog_hal_t runa_watchdog_mock_hal(runa_watchdog_mock_state_t *state) {
    runa_watchdog_hal_t hal = { state, status, arm, feed, disarm };
    return hal;
}
