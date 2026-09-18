#include "job_runtime.h"

#include "runa_core.h"
#if RUNA_MODULE_GPIO
#include "runa_gpio.h"
#endif
#if RUNA_MODULE_ADC
#include "runa_adc.h"
#endif
#if RUNA_MODULE_PWM
#include "runa_pwm.h"
#endif

#include <limits.h>

typedef struct legacy_hal_adapter {
    const job_hal_t *hal;
} legacy_hal_adapter_t;

#if RUNA_MODULE_GPIO || RUNA_MODULE_ADC || RUNA_MODULE_PWM
static runa_status_t convert_hal_status(job_hal_status_t status) {
    if (status == JOB_HAL_OK) return RUNA_OK;
    if (status == JOB_HAL_TIMEOUT) return RUNA_ERR_IO_TIMEOUT;
    return RUNA_ERR_CANCELLED;
}
#endif

#if RUNA_MODULE_GPIO
static runa_status_t legacy_gpio_read(void *context, uintptr_t handle, uint32_t *value) {
    legacy_hal_adapter_t *adapter = (legacy_hal_adapter_t *)context;
    if (adapter->hal == NULL || adapter->hal->gpio_read == NULL || handle > UINT32_MAX)
        return RUNA_ERR_INTERNAL;
    return convert_hal_status(adapter->hal->gpio_read(adapter->hal->context, (uint32_t)handle, value));
}

static runa_status_t legacy_gpio_write(void *context, uintptr_t handle, uint32_t value) {
    legacy_hal_adapter_t *adapter = (legacy_hal_adapter_t *)context;
    if (adapter->hal == NULL || adapter->hal->gpio_write == NULL || handle > UINT32_MAX)
        return RUNA_ERR_INTERNAL;
    return convert_hal_status(adapter->hal->gpio_write(adapter->hal->context, (uint32_t)handle, value));
}
#endif

#if RUNA_MODULE_ADC
static runa_status_t legacy_adc_read(void *context, uintptr_t handle, uint32_t *value) {
    legacy_hal_adapter_t *adapter = (legacy_hal_adapter_t *)context;
    if (adapter->hal == NULL || adapter->hal->adc_read == NULL || handle > UINT32_MAX)
        return RUNA_ERR_INTERNAL;
    return convert_hal_status(adapter->hal->adc_read(adapter->hal->context, (uint32_t)handle, value));
}
#endif

#if RUNA_MODULE_PWM
static runa_status_t legacy_pwm_write(void *context, uintptr_t handle, uint32_t value) {
    legacy_hal_adapter_t *adapter = (legacy_hal_adapter_t *)context;
    if (adapter->hal == NULL || adapter->hal->pwm_write == NULL || handle > UINT32_MAX)
        return RUNA_ERR_INTERNAL;
    return convert_hal_status(adapter->hal->pwm_write(adapter->hal->context, (uint32_t)handle, value));
}
#endif

static uint64_t legacy_time_us(void *context) {
    legacy_hal_adapter_t *adapter = (legacy_hal_adapter_t *)context;
    return adapter->hal->time_us(adapter->hal->context);
}

static runa_status_t legacy_delay_ms(void *context, uint32_t milliseconds) {
    legacy_hal_adapter_t *adapter = (legacy_hal_adapter_t *)context;
    job_hal_status_t status;
    if (adapter->hal == NULL || adapter->hal->delay_ms == NULL) return RUNA_ERR_INTERNAL;
    status = adapter->hal->delay_ms(adapter->hal->context, milliseconds);
    if (status == JOB_HAL_TIMEOUT) return RUNA_ERR_IO_TIMEOUT;
    return status == JOB_HAL_OK ? RUNA_OK : RUNA_ERR_INTERNAL;
}

job_execution_summary_t job_process(const uint8_t *data, size_t size,
                                    const job_resource_table_t *legacy_resources,
                                    const job_hal_t *legacy_hal,
                                    const job_event_sink_t *legacy_sink) {
    runa_resource_t resources[RUNA_MAX_RESOURCES];
#if RUNA_MODULE_ADC
    runa_adc_resource_config_t adc_config[RUNA_MAX_RESOURCES];
#endif
#if RUNA_MODULE_PWM
    runa_pwm_resource_config_t pwm_config[RUNA_MAX_RESOURCES];
#endif
    runa_resource_table_t resource_table;
    legacy_hal_adapter_t hal_adapter = { legacy_hal };
    runa_platform_t platform = { &hal_adapter, NULL, legacy_delay_ms };
    runa_event_sink_t sink;
    runa_module_registry_t registry;
    runa_execution_summary_t runa_summary;
    job_execution_summary_t summary;
    size_t index;
#if RUNA_MODULE_GPIO
    runa_gpio_hal_t gpio_hal;
    runa_module_t gpio_module;
#endif
#if RUNA_MODULE_ADC
    runa_adc_hal_t adc_hal;
    runa_module_t adc_module;
#endif
#if RUNA_MODULE_PWM
    runa_pwm_hal_t pwm_hal;
    runa_module_t pwm_module;
#endif
    if (legacy_resources == NULL || legacy_resources->count > RUNA_MAX_RESOURCES ||
        (legacy_resources->count != 0u && legacy_resources->items == NULL) ||
        legacy_sink == NULL || legacy_sink->send == NULL) {
        job_execution_summary_t invalid = { JOB_ERR_INVALID_FORMAT, 0u, 0u, 0u, 0u, 0u };
        return invalid;
    }
    for (index = 0u; index < legacy_resources->count; ++index) {
        const job_resource_t *source = &legacy_resources->items[index];
        runa_resource_t *target = &resources[index];
        target->id = source->id;
        target->resource_type = 1u;
        target->reserved = 0u;
        target->permissions = source->permissions;
        target->platform_handle = (uintptr_t)source->platform_handle;
        target->config = NULL;
        switch (source->type) {
        case JOB_RESOURCE_GPIO:
            target->module_id = 1u;
            break;
        case JOB_RESOURCE_ADC:
            target->module_id = 2u;
#if RUNA_MODULE_ADC
            adc_config[index].maximum_value = source->maximum_value;
            target->config = &adc_config[index];
#endif
            break;
        case JOB_RESOURCE_PWM:
            target->module_id = 3u;
#if RUNA_MODULE_PWM
            pwm_config[index].maximum_value = source->maximum_value != 0u ?
                                               source->maximum_value : UINT32_MAX;
            target->config = &pwm_config[index];
#endif
            break;
        default:
            target->module_id = 0u;
            break;
        }
    }
    resource_table.items = resources;
    resource_table.count = legacy_resources->count;
    runa_registry_init(&registry);
#if RUNA_MODULE_GPIO
    gpio_hal.context = &hal_adapter;
    gpio_hal.read = legacy_gpio_read;
    gpio_hal.write = legacy_gpio_write;
    gpio_module = runa_gpio_module(&gpio_hal);
    if (runa_registry_add(&registry, &gpio_module) != RUNA_OK) {
        job_execution_summary_t invalid = { JOB_ERR_INTERNAL, 0u, 0u, 0u, 0u, 0u };
        return invalid;
    }
#endif
#if RUNA_MODULE_ADC
    adc_hal.context = &hal_adapter;
    adc_hal.read = legacy_adc_read;
    adc_module = runa_adc_module(&adc_hal);
    if (runa_registry_add(&registry, &adc_module) != RUNA_OK) {
        job_execution_summary_t invalid = { JOB_ERR_INTERNAL, 0u, 0u, 0u, 0u, 0u };
        return invalid;
    }
#endif
#if RUNA_MODULE_PWM
    pwm_hal.context = &hal_adapter;
    pwm_hal.write = legacy_pwm_write;
    pwm_module = runa_pwm_module(&pwm_hal);
    if (runa_registry_add(&registry, &pwm_module) != RUNA_OK) {
        job_execution_summary_t invalid = { JOB_ERR_INTERNAL, 0u, 0u, 0u, 0u, 0u };
        return invalid;
    }
#endif
    platform.time_us = legacy_hal != NULL && legacy_hal->time_us != NULL ? legacy_time_us : NULL;
    sink.send = legacy_sink->send;
    sink.context = legacy_sink->context;
    runa_summary = runa_process(data, size, &resource_table, &registry, &platform, &sink);
    summary.error = (job_error_t)runa_summary.error;
    summary.steps = runa_summary.steps;
    summary.emit_count = runa_summary.emit_count;
    summary.emit_bytes = runa_summary.emit_bytes;
    summary.accepted = runa_summary.accepted;
    summary.result_sent = runa_summary.result_sent;
    return summary;
}
