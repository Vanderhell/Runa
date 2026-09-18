#ifndef RUNA_PWM_H
#define RUNA_PWM_H

#include "runa_module.h"

#define RUNA_PWM_MODULE_ID 3u
#define RUNA_PWM_RESOURCE_TYPE 1u
#define RUNA_PWM_OP_WRITE 1u

enum { RUNA_PWM_ERR_IO = RUNA_ERR_MODULE_BASE + 2u };

typedef struct runa_pwm_resource_config {
    uint32_t maximum_value;
} runa_pwm_resource_config_t;

typedef struct runa_pwm_hal {
    void *context;
    runa_status_t (*write)(void *context, uintptr_t handle, uint32_t value);
} runa_pwm_hal_t;

runa_module_t runa_pwm_module(runa_pwm_hal_t *hal);

#endif
