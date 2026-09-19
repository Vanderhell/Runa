#ifndef RUNA_WATCHDOG_H
#define RUNA_WATCHDOG_H

#include "runa_module.h"

#define RUNA_WATCHDOG_MODULE_ID 14u
#define RUNA_WATCHDOG_RESOURCE_TYPE 1u

#define RUNA_WATCHDOG_OP_GET_STATUS 1u
#define RUNA_WATCHDOG_OP_ARM 2u
#define RUNA_WATCHDOG_OP_FEED 3u
#define RUNA_WATCHDOG_OP_DISARM 4u

#define RUNA_WATCHDOG_SUPPORT_STATUS 0x01u
#define RUNA_WATCHDOG_SUPPORT_ARM 0x02u
#define RUNA_WATCHDOG_SUPPORT_FEED 0x04u
#define RUNA_WATCHDOG_SUPPORT_DISARM 0x08u

#define RUNA_WATCHDOG_STATUS_SUPPORTED 0x01u
#define RUNA_WATCHDOG_STATUS_ARMED 0x02u
#define RUNA_WATCHDOG_STATUS_DISARM_SUPPORTED 0x04u
#define RUNA_WATCHDOG_STATUS_WINDOWED 0x08u
#define RUNA_WATCHDOG_STATUS_LAST_RESET_WATCHDOG 0x10u
#define RUNA_WATCHDOG_STATUS_TIMEOUT_CONFIGURED 0x20u

#define RUNA_WATCHDOG_STATUS_DATA_VERSION 1u
#define RUNA_WATCHDOG_CAPABILITY_VERSION 1u
#define RUNA_WATCHDOG_CAPABILITY_SIZE 16u
#define RUNA_WATCHDOG_STATUS_DATA_SIZE 8u
#define RUNA_WATCHDOG_MAX_TIMEOUT_MS 86400000u

enum {
    RUNA_WATCHDOG_ERR_IO = RUNA_ERR_MODULE_BASE + 32u,
    RUNA_WATCHDOG_ERR_INVALID_STATUS = RUNA_ERR_MODULE_BASE + 33u,
    RUNA_WATCHDOG_ERR_UNSUPPORTED = RUNA_ERR_MODULE_BASE + 34u
};

typedef struct runa_watchdog_resource_config {
    uint32_t minimum_timeout_ms;
    uint32_t maximum_timeout_ms;
    uint32_t default_timeout_ms;
    uint8_t supported_operations;
    uint8_t windowed;
    uint8_t last_reset_supported;
    uint8_t reserved;
} runa_watchdog_resource_config_t;

typedef struct runa_watchdog_status {
    uint8_t flags;
    uint8_t reserved[3];
    uint32_t timeout_ms;
} runa_watchdog_status_t;

typedef struct runa_watchdog_hal {
    void *context;
    runa_status_t (*status)(void *context, uintptr_t handle,
                            runa_watchdog_status_t *status);
    runa_status_t (*arm)(void *context, uintptr_t handle, uint32_t timeout_ms);
    runa_status_t (*feed)(void *context, uintptr_t handle);
    runa_status_t (*disarm)(void *context, uintptr_t handle);
} runa_watchdog_hal_t;

runa_module_t runa_watchdog_module(runa_watchdog_hal_t *hal);

#endif
