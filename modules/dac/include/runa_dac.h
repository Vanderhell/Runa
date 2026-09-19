#ifndef RUNA_DAC_H
#define RUNA_DAC_H

#include "runa_module.h"

#define RUNA_DAC_MODULE_ID 9u
#define RUNA_DAC_RESOURCE_TYPE 1u
#define RUNA_DAC_OP_WRITE 1u
#define RUNA_DAC_VALUE_MODEL_NATIVE 0u
#define RUNA_DAC_MAX_RESOLUTION_BITS 32u
#define RUNA_DAC_CAPABILITY_SIZE 4u

enum { RUNA_DAC_ERR_IO = 32u };

typedef struct runa_dac_resource_config {
    uint32_t maximum_value;
    uint8_t resolution_bits;
    uint8_t reserved[3];
} runa_dac_resource_config_t;

typedef struct runa_dac_hal {
    void *context;
    runa_status_t (*write)(void *context, uintptr_t handle, uint32_t value);
} runa_dac_hal_t;

runa_module_t runa_dac_module(runa_dac_hal_t *hal);

#endif
