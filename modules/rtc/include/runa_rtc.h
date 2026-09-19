#ifndef RUNA_RTC_H
#define RUNA_RTC_H

#include <stdint.h>

#include "runa_module.h"

#define RUNA_RTC_MODULE_ID 13u
#define RUNA_RTC_RESOURCE_TYPE 1u
#define RUNA_RTC_OP_READ 1u
#define RUNA_RTC_OP_SET 2u
#define RUNA_RTC_OP_GET_STATUS 3u

/* POSIX/Unix seconds are UTC wall-clock seconds. Leap seconds are not
 * represented separately. The range ends at 9999-12-31T23:59:59Z. */
#define RUNA_RTC_MIN_SECONDS UINT64_C(0)
#define RUNA_RTC_MAX_SECONDS UINT64_C(253402300799)

#define RUNA_RTC_RESULT_VERSION 1u
#define RUNA_RTC_RESULT_READ 1u
#define RUNA_RTC_RESULT_STATUS 2u
#define RUNA_RTC_RESULT_TIME_BYTES 16u
#define RUNA_RTC_RESULT_STATUS_BYTES 8u
#define RUNA_RTC_CAPABILITY_PAYLOAD_SIZE 20u

enum {
    RUNA_RTC_STATUS_VALID_TIME = 1u << 0,
    RUNA_RTC_STATUS_POWER_LOSS_DETECTED = 1u << 1,
    RUNA_RTC_STATUS_OSCILLATOR_STOPPED = 1u << 2,
    RUNA_RTC_STATUS_BATTERY_BACKED = 1u << 3,
    RUNA_RTC_STATUS_SET_SUPPORTED = 1u << 4
};

#define RUNA_RTC_RUNTIME_STATUS_MASK \
    (RUNA_RTC_STATUS_VALID_TIME | RUNA_RTC_STATUS_POWER_LOSS_DETECTED | \
     RUNA_RTC_STATUS_OSCILLATOR_STOPPED)

enum {
    RUNA_RTC_CAP_SET_SUPPORTED = 1u << 0,
    RUNA_RTC_CAP_BATTERY_BACKED = 1u << 1,
    RUNA_RTC_CAP_RETAINED_RESET = 1u << 2,
    RUNA_RTC_CAP_RETAINED_DEEP_SLEEP = 1u << 3,
    RUNA_RTC_CAP_RETAINED_POWER_LOSS = 1u << 4
};

#define RUNA_RTC_CAPABILITY_FLAGS_MASK \
    (RUNA_RTC_CAP_SET_SUPPORTED | RUNA_RTC_CAP_BATTERY_BACKED | \
     RUNA_RTC_CAP_RETAINED_RESET | RUNA_RTC_CAP_RETAINED_DEEP_SLEEP | \
     RUNA_RTC_CAP_RETAINED_POWER_LOSS)

enum {
    RUNA_RTC_ERR_SET_UNSUPPORTED = RUNA_ERR_MODULE_BASE + 32u,
    RUNA_RTC_ERR_STATUS_FLAGS = RUNA_ERR_MODULE_BASE + 33u,
    RUNA_RTC_ERR_UNSUPPORTED = RUNA_ERR_MODULE_BASE + 34u
};

typedef struct runa_rtc_time {
    uint64_t seconds;
} runa_rtc_time_t;

typedef struct runa_rtc_resource_config {
    uint64_t minimum_seconds;
    uint64_t maximum_seconds;
    uint32_t capability_flags;
    uint32_t supported_status_flags;
} runa_rtc_resource_config_t;

typedef struct runa_rtc_hal {
    void *context;
    runa_status_t (*read)(void *context, uintptr_t handle, runa_rtc_time_t *time);
    runa_status_t (*set)(void *context, uintptr_t handle, const runa_rtc_time_t *time);
    runa_status_t (*status)(void *context, uintptr_t handle, uint32_t *status_flags);
} runa_rtc_hal_t;

runa_module_t runa_rtc_module(runa_rtc_hal_t *hal);

#endif
