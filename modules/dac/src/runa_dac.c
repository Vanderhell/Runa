#include "runa_dac.h"
#include "runa_ir.h"
#include "runa_resource.h"

static runa_status_t validate_resource(void *context, const runa_resource_t *resource, uint32_t *detail) {
    const runa_dac_resource_config_t *configuration;
    (void)context;
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (detail != NULL) *detail = resource->id;
    if (resource->resource_type != RUNA_DAC_RESOURCE_TYPE) return RUNA_ERR_RESOURCE_TYPE;
    configuration = (const runa_dac_resource_config_t *)resource->config;
    if (configuration == NULL || configuration->maximum_value == 0u || configuration->resolution_bits == 0u ||
        configuration->resolution_bits > RUNA_DAC_MAX_RESOLUTION_BITS || configuration->reserved[0] != 0u ||
        configuration->reserved[1] != 0u || configuration->reserved[2] != 0u) return RUNA_ERR_INVALID_RESOURCE;
    return RUNA_OK;
}

static runa_status_t validate(void *context, const runa_module_job_t *job,
                              const runa_module_instruction_t *instruction, uint32_t *detail) {
    const runa_resource_t *resource;
    uint16_t resource_id;
    uint8_t reg;
    (void)context;
    if (instruction == NULL || instruction->operands == NULL || instruction->operand_size != 3u)
        return RUNA_ERR_INVALID_OPERAND;
    if (instruction->operation != RUNA_DAC_OP_WRITE) return RUNA_ERR_INVALID_OPCODE;
    resource_id = runa_read_u16_le(instruction->operands);
    reg = instruction->operands[2];
    if (detail != NULL) *detail = resource_id;
    if (job == NULL || reg >= job->register_count) return RUNA_ERR_INVALID_REGISTER;
    resource = runa_resource_find(job->resources, resource_id);
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (resource->module_id != RUNA_DAC_MODULE_ID || resource->resource_type != RUNA_DAC_RESOURCE_TYPE)
        return RUNA_ERR_RESOURCE_TYPE;
    if ((resource->permissions & RUNA_PERMISSION_WRITE) == 0u) return RUNA_ERR_ACCESS_DENIED;
    return validate_resource(NULL, resource, detail);
}

static runa_status_t execute(void *context, runa_module_job_t *job,
                             const runa_module_instruction_t *instruction, uint32_t *detail) {
    runa_dac_hal_t *hal = (runa_dac_hal_t *)context;
    uint16_t resource_id = runa_read_u16_le(instruction->operands);
    uint8_t reg = instruction->operands[2];
    const runa_resource_t *resource = runa_resource_find(job->resources, resource_id);
    const runa_dac_resource_config_t *configuration;
    uint32_t value = job->registers[reg];
    runa_status_t status;
    if (detail != NULL) *detail = resource_id;
    if (resource == NULL || resource->config == NULL || hal == NULL || hal->write == NULL) return RUNA_ERR_INTERNAL;
    configuration = (const runa_dac_resource_config_t *)resource->config;
    if (value > configuration->maximum_value) {
        if (detail != NULL) *detail = value;
        return RUNA_ERR_OUT_OF_RANGE;
    }
    status = hal->write(hal->context, resource->platform_handle, value);
    return status == RUNA_OK || status == RUNA_ERR_IO_TIMEOUT || status == RUNA_ERR_INTERNAL ? status : RUNA_DAC_ERR_IO;
}

static size_t capabilities(void *context, uint8_t *output, size_t capacity) {
    (void)context;
    if (output != NULL && capacity >= RUNA_DAC_CAPABILITY_SIZE) {
        output[0] = RUNA_DAC_OP_WRITE;
        output[1] = RUNA_DAC_VALUE_MODEL_NATIVE;
        output[2] = RUNA_DAC_MAX_RESOLUTION_BITS;
        output[3] = 0u;
    }
    return RUNA_DAC_CAPABILITY_SIZE;
}

runa_module_t runa_dac_module(runa_dac_hal_t *hal) {
    runa_module_t module = { RUNA_DAC_MODULE_ID, RUNA_MODULE_ABI_VERSION, 1u,
                             validate, execute, NULL, NULL, capabilities, hal,
                             NULL, 0u, validate_resource };
    return module;
}
