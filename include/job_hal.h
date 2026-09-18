#ifndef JOB_HAL_H
#define JOB_HAL_H

#include <stdint.h>

typedef enum job_hal_status {
    JOB_HAL_OK = 0,
    JOB_HAL_FAILURE = 1,
    JOB_HAL_TIMEOUT = 2
} job_hal_status_t;

typedef struct job_hal {
    void *context;
    job_hal_status_t (*gpio_read)(void *context, uint32_t handle, uint32_t *value);
    job_hal_status_t (*gpio_write)(void *context, uint32_t handle, uint32_t value);
    job_hal_status_t (*adc_read)(void *context, uint32_t handle, uint32_t *value);
    job_hal_status_t (*pwm_write)(void *context, uint32_t handle, uint32_t value);
    uint64_t (*time_us)(void *context);
    job_hal_status_t (*delay_ms)(void *context, uint32_t milliseconds);
} job_hal_t;

#endif

