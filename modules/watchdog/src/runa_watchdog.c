#include "runa_watchdog.h"

#include "runa_ir.h"
#include "runa_resource.h"

static runa_status_t validate_resource(void *context, const runa_resource_t *resource,
                                       uint32_t *detail) {
    const runa_watchdog_resource_config_t *configuration;
    (void)context;
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (detail != NULL) *detail = resource->id;
    if (resource->module_id != RUNA_WATCHDOG_MODULE_ID ||
        resource->resource_type != RUNA_WATCHDOG_RESOURCE_TYPE)
        return RUNA_ERR_RESOURCE_TYPE;
    configuration = (const runa_watchdog_resource_config_t *)resource->config;
    if (configuration == NULL || configuration->minimum_timeout_ms == 0u ||
        configuration->maximum_timeout_ms < configuration->minimum_timeout_ms ||
        configuration->maximum_timeout_ms > RUNA_WATCHDOG_MAX_TIMEOUT_MS ||
        (configuration->default_timeout_ms != 0u &&
         (configuration->default_timeout_ms < configuration->minimum_timeout_ms ||
          configuration->default_timeout_ms > configuration->maximum_timeout_ms)) ||
        configuration->supported_operations > 0x0fu ||
        configuration->windowed != 0u || configuration->last_reset_supported > 1u ||
        configuration->reserved != 0u)
        return RUNA_ERR_INVALID_RESOURCE;
    return RUNA_OK;
}

static const runa_watchdog_resource_config_t *configuration_for(
    const runa_module_job_t *job, const runa_module_instruction_t *instruction,
    const runa_resource_t **resource, uint32_t *detail) {
    uint16_t resource_id;
    if (job == NULL || instruction == NULL || instruction->operands == NULL ||
        instruction->operand_size < 2u) return NULL;
    resource_id = runa_read_u16_le(instruction->operands);
    if (detail != NULL) *detail = resource_id;
    *resource = runa_resource_find(job->resources, resource_id);
    if (*resource == NULL || (*resource)->module_id != RUNA_WATCHDOG_MODULE_ID ||
        (*resource)->resource_type != RUNA_WATCHDOG_RESOURCE_TYPE ||
        validate_resource(NULL, *resource, detail) != RUNA_OK)
        return NULL;
    return (const runa_watchdog_resource_config_t *)(*resource)->config;
}

static uint8_t required_permission(uint8_t operation) {
    return operation == RUNA_WATCHDOG_OP_GET_STATUS ? RUNA_PERMISSION_READ : RUNA_PERMISSION_WRITE;
}

static uint8_t required_support(uint8_t operation) {
    switch (operation) {
    case RUNA_WATCHDOG_OP_GET_STATUS: return RUNA_WATCHDOG_SUPPORT_STATUS;
    case RUNA_WATCHDOG_OP_ARM: return RUNA_WATCHDOG_SUPPORT_ARM;
    case RUNA_WATCHDOG_OP_FEED: return RUNA_WATCHDOG_SUPPORT_FEED;
    case RUNA_WATCHDOG_OP_DISARM: return RUNA_WATCHDOG_SUPPORT_DISARM;
    default: return 0u;
    }
}

static runa_status_t validate_common(void *context, const runa_module_job_t *job,
                                     const runa_module_instruction_t *instruction,
                                     const runa_resource_t **resource,
                                     const runa_watchdog_resource_config_t **configuration,
                                     uint32_t *detail) {
    runa_watchdog_hal_t *hal = (runa_watchdog_hal_t *)context;
    uint8_t permission;
    uint8_t support;
    *configuration = configuration_for(job, instruction, resource, detail);
    if (*configuration == NULL) {
        if (runa_resource_find(job == NULL ? NULL : job->resources,
                               instruction == NULL || instruction->operands == NULL ? 0u :
                               runa_read_u16_le(instruction->operands)) == NULL)
            return RUNA_ERR_INVALID_RESOURCE;
        return RUNA_ERR_RESOURCE_TYPE;
    }
    permission = required_permission(instruction->operation);
    support = required_support(instruction->operation);
    if (((*resource)->permissions & permission) == 0u) return RUNA_ERR_ACCESS_DENIED;
    if (support == 0u || ((*configuration)->supported_operations & support) == 0u)
        return RUNA_WATCHDOG_ERR_UNSUPPORTED;
    if (hal == NULL) return RUNA_ERR_INTERNAL;
    if ((instruction->operation == RUNA_WATCHDOG_OP_GET_STATUS && hal->status == NULL) ||
        (instruction->operation == RUNA_WATCHDOG_OP_ARM && hal->arm == NULL) ||
        (instruction->operation == RUNA_WATCHDOG_OP_FEED && hal->feed == NULL) ||
        (instruction->operation == RUNA_WATCHDOG_OP_DISARM && hal->disarm == NULL))
        return RUNA_WATCHDOG_ERR_UNSUPPORTED;
    return RUNA_OK;
}

static runa_status_t validate(void *context, const runa_module_job_t *job,
                              const runa_module_instruction_t *instruction, uint32_t *detail) {
    const runa_resource_t *resource;
    const runa_watchdog_resource_config_t *configuration;
    runa_status_t status;
    uint32_t timeout_ms;
    if (instruction == NULL || instruction->operands == NULL) return RUNA_ERR_INVALID_OPERAND;
    if ((instruction->operation == RUNA_WATCHDOG_OP_GET_STATUS ||
         instruction->operation == RUNA_WATCHDOG_OP_FEED ||
         instruction->operation == RUNA_WATCHDOG_OP_DISARM) && instruction->operand_size != 2u)
        return RUNA_ERR_INVALID_OPERAND;
    if (instruction->operation == RUNA_WATCHDOG_OP_ARM && instruction->operand_size != 6u)
        return RUNA_ERR_INVALID_OPERAND;
    if (instruction->operation < RUNA_WATCHDOG_OP_GET_STATUS ||
        instruction->operation > RUNA_WATCHDOG_OP_DISARM)
        return RUNA_ERR_INVALID_OPCODE;
    status = validate_common(context, job, instruction, &resource, &configuration, detail);
    if (status != RUNA_OK) return status;
    (void)resource;
    if (instruction->operation == RUNA_WATCHDOG_OP_ARM) {
        timeout_ms = runa_read_u32_le(instruction->operands + 2u);
        if (timeout_ms < configuration->minimum_timeout_ms ||
            timeout_ms > configuration->maximum_timeout_ms)
            return RUNA_ERR_OUT_OF_RANGE;
    }
    return RUNA_OK;
}

static runa_status_t map_hal_status(runa_status_t status) {
    if (status == RUNA_OK || status == RUNA_ERR_IO_TIMEOUT ||
        status == RUNA_ERR_CANCELLED || status == RUNA_ERR_INTERNAL ||
        status == RUNA_WATCHDOG_ERR_IO || status == RUNA_WATCHDOG_ERR_UNSUPPORTED)
        return status;
    return RUNA_WATCHDOG_ERR_IO;
}

static runa_status_t normalize_status(const runa_watchdog_resource_config_t *configuration,
                                      runa_watchdog_status_t *status) {
    uint8_t dynamic_flags = RUNA_WATCHDOG_STATUS_ARMED |
                             RUNA_WATCHDOG_STATUS_LAST_RESET_WATCHDOG |
                             RUNA_WATCHDOG_STATUS_TIMEOUT_CONFIGURED;
    if (status->reserved[0] != 0u || status->reserved[1] != 0u || status->reserved[2] != 0u ||
        (status->flags & (uint8_t)(0xffu ^ dynamic_flags)) != 0u ||
        (status->timeout_ms != 0u &&
         (status->timeout_ms < configuration->minimum_timeout_ms ||
          status->timeout_ms > configuration->maximum_timeout_ms)))
        return RUNA_WATCHDOG_ERR_INVALID_STATUS;
    if (configuration->last_reset_supported == 0u)
        status->flags &= (uint8_t)(0xffu ^ RUNA_WATCHDOG_STATUS_LAST_RESET_WATCHDOG);
    status->flags |= RUNA_WATCHDOG_STATUS_SUPPORTED;
    if ((configuration->supported_operations & RUNA_WATCHDOG_SUPPORT_DISARM) != 0u)
        status->flags |= RUNA_WATCHDOG_STATUS_DISARM_SUPPORTED;
    if (configuration->windowed != 0u) status->flags |= RUNA_WATCHDOG_STATUS_WINDOWED;
    if (status->timeout_ms != 0u) status->flags |= RUNA_WATCHDOG_STATUS_TIMEOUT_CONFIGURED;
    return RUNA_OK;
}

static runa_status_t execute(void *context, runa_module_job_t *job,
                             const runa_module_instruction_t *instruction, uint32_t *detail) {
    runa_watchdog_hal_t *hal = (runa_watchdog_hal_t *)context;
    const runa_resource_t *resource;
    const runa_watchdog_resource_config_t *configuration;
    runa_watchdog_status_t watchdog_status;
    uint8_t status_data[RUNA_WATCHDOG_STATUS_DATA_SIZE];
    runa_status_t status;
    status = validate_common(context, job, instruction, &resource, &configuration, detail);
    if (status != RUNA_OK) return status;
    if (instruction->operation == RUNA_WATCHDOG_OP_GET_STATUS) {
        watchdog_status.flags = 0u;
        watchdog_status.reserved[0] = 0u;
        watchdog_status.reserved[1] = 0u;
        watchdog_status.reserved[2] = 0u;
        watchdog_status.timeout_ms = 0u;
        status = map_hal_status(hal->status(hal->context, resource->platform_handle,
                                             &watchdog_status));
        if (status != RUNA_OK) return status;
        status = normalize_status(configuration, &watchdog_status);
        if (status != RUNA_OK) return status;
        if (job->emit_data == NULL) return RUNA_ERR_INTERNAL;
        status_data[0] = RUNA_WATCHDOG_STATUS_DATA_VERSION;
        status_data[1] = watchdog_status.flags;
        status_data[2] = 0u;
        status_data[3] = 0u;
        runa_write_u32_le(status_data + 4u, watchdog_status.timeout_ms);
        return job->emit_data(job->emit_context, RUNA_WATCHDOG_MODULE_ID,
                              instruction->instruction_index, 0u,
                              status_data, sizeof status_data);
    }
    if (instruction->operation == RUNA_WATCHDOG_OP_ARM)
        status = hal->arm(hal->context, resource->platform_handle,
                          runa_read_u32_le(instruction->operands + 2u));
    else if (instruction->operation == RUNA_WATCHDOG_OP_FEED)
        status = hal->feed(hal->context, resource->platform_handle);
    else
        status = hal->disarm(hal->context, resource->platform_handle);
    return map_hal_status(status);
}

static size_t capabilities(void *context, uint8_t *output, size_t capacity) {
    runa_watchdog_hal_t *hal = (runa_watchdog_hal_t *)context;
    uint8_t operations = 0u;
    if (hal != NULL) {
        if (hal->status != NULL) operations |= RUNA_WATCHDOG_SUPPORT_STATUS;
        if (hal->arm != NULL) operations |= RUNA_WATCHDOG_SUPPORT_ARM;
        if (hal->feed != NULL) operations |= RUNA_WATCHDOG_SUPPORT_FEED;
        if (hal->disarm != NULL) operations |= RUNA_WATCHDOG_SUPPORT_DISARM;
    }
    if (output != NULL && capacity >= RUNA_WATCHDOG_CAPABILITY_SIZE) {
        output[0] = RUNA_WATCHDOG_CAPABILITY_VERSION;
        output[1] = operations;
        output[2] = RUNA_WATCHDOG_STATUS_DATA_VERSION;
        output[3] = 0u;
        runa_write_u32_le(output + 4u, 1u);
        runa_write_u32_le(output + 8u, RUNA_WATCHDOG_MAX_TIMEOUT_MS);
        runa_write_u32_le(output + 12u, 0u);
    }
    return RUNA_WATCHDOG_CAPABILITY_SIZE;
}

runa_module_t runa_watchdog_module(runa_watchdog_hal_t *hal) {
    runa_module_t module = { RUNA_WATCHDOG_MODULE_ID, RUNA_MODULE_ABI_VERSION, 1u,
                             validate, execute, NULL, NULL, capabilities, hal,
                             NULL, 0u, validate_resource };
    return module;
}
