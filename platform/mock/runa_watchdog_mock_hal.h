#ifndef RUNA_WATCHDOG_MOCK_HAL_H
#define RUNA_WATCHDOG_MOCK_HAL_H

#include "runa_watchdog.h"

typedef struct runa_watchdog_mock_state {
    uint8_t armed;
    uint8_t last_reset_watchdog;
    uint8_t disarm_supported;
    uint8_t reserved;
    uint32_t timeout_ms;
    uint32_t feed_count;
    uint32_t status_calls;
    uint32_t arm_calls;
    uint32_t feed_calls;
    uint32_t disarm_calls;
    runa_status_t next_status;
} runa_watchdog_mock_state_t;

void runa_watchdog_mock_init(runa_watchdog_mock_state_t *state);
runa_watchdog_hal_t runa_watchdog_mock_hal(runa_watchdog_mock_state_t *state);

#endif
