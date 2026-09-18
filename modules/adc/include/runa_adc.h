#ifndef RUNA_ADC_H
#define RUNA_ADC_H

#include "runa_module.h"

#define RUNA_ADC_MODULE_ID 2u
#define RUNA_ADC_RESOURCE_TYPE 1u
#define RUNA_ADC_OP_READ 1u

enum { RUNA_ADC_ERR_IO = RUNA_ERR_MODULE_BASE + 1u };

typedef struct runa_adc_resource_config {
    uint32_t maximum_value;
} runa_adc_resource_config_t;

typedef struct runa_adc_hal {
    void *context;
    runa_status_t (*read)(void *context, uintptr_t handle, uint32_t *value);
} runa_adc_hal_t;

runa_module_t runa_adc_module(runa_adc_hal_t *hal);

#endif
