#ifndef RUNA_GPIO_H
#define RUNA_GPIO_H

#include "runa_module.h"

#define RUNA_GPIO_MODULE_ID 1u
#define RUNA_GPIO_RESOURCE_TYPE 1u
#define RUNA_GPIO_OP_READ 1u
#define RUNA_GPIO_OP_WRITE 2u

enum { RUNA_GPIO_ERR_IO = RUNA_ERR_MODULE_BASE };

typedef struct runa_gpio_hal {
    void *context;
    runa_status_t (*read)(void *context, uintptr_t handle, uint32_t *value);
    runa_status_t (*write)(void *context, uintptr_t handle, uint32_t value);
} runa_gpio_hal_t;

runa_module_t runa_gpio_module(runa_gpio_hal_t *hal);

#endif
