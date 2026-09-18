#include "runa_pwm.h"
#include "runa_ir.h"
#include "runa_resource.h"

static runa_status_t validate_resource(void *context, const runa_resource_t *resource,
                                      uint32_t *detail) {
    const runa_pwm_resource_config_t *configuration;
    (void)context;
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (detail != NULL) *detail = resource->id;
    if (resource->resource_type != RUNA_PWM_RESOURCE_TYPE) return RUNA_ERR_RESOURCE_TYPE;
    configuration = (const runa_pwm_resource_config_t *)resource->config;
    return configuration != NULL && configuration->maximum_value != 0u ?
           RUNA_OK : RUNA_ERR_INVALID_RESOURCE;
}

static runa_status_t validate(void *context, const runa_module_job_t *job,
                              const runa_module_instruction_t *instruction, uint32_t *detail) {
    const runa_resource_t *resource;
    uint16_t id;
    uint8_t reg;
    (void)context;
    if (instruction == NULL || instruction->operands == NULL || instruction->operand_size != 3u)
        return RUNA_ERR_INVALID_OPERAND;
    if (instruction->operation != RUNA_PWM_OP_WRITE) return RUNA_ERR_INVALID_OPCODE;
    id = runa_read_u16_le(instruction->operands);
    reg = instruction->operands[2];
    if (detail != NULL) *detail = id;
    if (job == NULL || reg >= job->register_count) return RUNA_ERR_INVALID_REGISTER;
    resource = runa_resource_find(job->resources, id);
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (resource->module_id != RUNA_PWM_MODULE_ID || resource->resource_type != RUNA_PWM_RESOURCE_TYPE)
        return RUNA_ERR_RESOURCE_TYPE;
    if ((resource->permissions & RUNA_PERMISSION_WRITE) == 0u) return RUNA_ERR_ACCESS_DENIED;
    if (resource->config == NULL || ((const runa_pwm_resource_config_t *)resource->config)->maximum_value == 0u)
        return RUNA_ERR_INVALID_RESOURCE;
    return RUNA_OK;
}

static runa_status_t execute(void *context, runa_module_job_t *job,
                             const runa_module_instruction_t *instruction, uint32_t *detail) {
    runa_pwm_hal_t *hal = (runa_pwm_hal_t *)context;
    uint16_t id = runa_read_u16_le(instruction->operands);
    uint8_t reg = instruction->operands[2];
    const runa_resource_t *resource = runa_resource_find(job->resources, id);
    uint32_t value = job->registers[reg];
    runa_status_t status;
    if (detail != NULL) *detail = id;
    if (resource == NULL || hal == NULL || hal->write == NULL) return RUNA_ERR_INTERNAL;
    if (value > ((const runa_pwm_resource_config_t *)resource->config)->maximum_value) {
        if (detail != NULL) *detail = value;
        return RUNA_ERR_OUT_OF_RANGE;
    }
    status = hal->write(hal->context, resource->platform_handle, value);
    return status == RUNA_OK || status == RUNA_ERR_IO_TIMEOUT || status == RUNA_ERR_INTERNAL ?
           status : RUNA_PWM_ERR_IO;
}

static size_t capabilities(void *context, uint8_t *output, size_t capacity) {
    (void)context;
    if (output != NULL && capacity >= 1u) output[0] = RUNA_PWM_OP_WRITE;
    return 1u;
}

static const runa_legacy_operation_t legacy_operations[] = { { 0x33u, RUNA_PWM_OP_WRITE } };

runa_module_t runa_pwm_module(runa_pwm_hal_t *hal) {
    runa_module_t module = { RUNA_PWM_MODULE_ID, RUNA_MODULE_ABI_VERSION, 1u,
                             validate, execute, NULL, NULL, capabilities, hal,
                             legacy_operations, (uint8_t)(sizeof legacy_operations / sizeof legacy_operations[0]),
                             validate_resource };
    return module;
}
