#include "runa_adc.h"
#include "runa_ir.h"
#include "runa_resource.h"

static runa_status_t validate_resource(void *context, const runa_resource_t *resource,
                                      uint32_t *detail) {
    (void)context;
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (detail != NULL) *detail = resource->id;
    return resource->resource_type == RUNA_ADC_RESOURCE_TYPE ? RUNA_OK : RUNA_ERR_RESOURCE_TYPE;
}

static runa_status_t validate(void *context, const runa_module_job_t *job,
                              const runa_module_instruction_t *instruction, uint32_t *detail) {
    const runa_resource_t *resource;
    uint16_t id;
    uint8_t reg;
    (void)context;
    if (instruction == NULL || instruction->operands == NULL || instruction->operand_size != 3u)
        return RUNA_ERR_INVALID_OPERAND;
    if (instruction->operation != RUNA_ADC_OP_READ) return RUNA_ERR_INVALID_OPCODE;
    id = runa_read_u16_le(instruction->operands);
    reg = instruction->operands[2];
    if (detail != NULL) *detail = id;
    if (job == NULL || reg >= job->register_count) return RUNA_ERR_INVALID_REGISTER;
    resource = runa_resource_find(job->resources, id);
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (resource->module_id != RUNA_ADC_MODULE_ID || resource->resource_type != RUNA_ADC_RESOURCE_TYPE)
        return RUNA_ERR_RESOURCE_TYPE;
    if ((resource->permissions & RUNA_PERMISSION_READ) == 0u) return RUNA_ERR_ACCESS_DENIED;
    return RUNA_OK;
}

static runa_status_t execute(void *context, runa_module_job_t *job,
                             const runa_module_instruction_t *instruction, uint32_t *detail) {
    runa_adc_hal_t *hal = (runa_adc_hal_t *)context;
    uint16_t id = runa_read_u16_le(instruction->operands);
    uint8_t reg = instruction->operands[2];
    const runa_resource_t *resource = runa_resource_find(job->resources, id);
    uint32_t value = 0u;
    runa_status_t status;
    if (detail != NULL) *detail = id;
    if (resource == NULL || hal == NULL || hal->read == NULL) return RUNA_ERR_INTERNAL;
    status = hal->read(hal->context, resource->platform_handle, &value);
    if (status != RUNA_OK)
        return status == RUNA_ERR_IO_TIMEOUT || status == RUNA_ERR_INTERNAL ? status : RUNA_ADC_ERR_IO;
    if (resource->config != NULL &&
        ((const runa_adc_resource_config_t *)resource->config)->maximum_value != 0u &&
        value > ((const runa_adc_resource_config_t *)resource->config)->maximum_value)
        return RUNA_ERR_OUT_OF_RANGE;
    job->registers[reg] = value;
    return RUNA_OK;
}

static size_t capabilities(void *context, uint8_t *output, size_t capacity) {
    (void)context;
    if (output != NULL && capacity >= 1u) output[0] = RUNA_ADC_OP_READ;
    return 1u;
}

static const runa_legacy_operation_t legacy_operations[] = { { 0x32u, RUNA_ADC_OP_READ } };

runa_module_t runa_adc_module(runa_adc_hal_t *hal) {
    runa_module_t module = { RUNA_ADC_MODULE_ID, RUNA_MODULE_ABI_VERSION, 1u,
                             validate, execute, NULL, NULL, capabilities, hal,
                             legacy_operations, (uint8_t)(sizeof legacy_operations / sizeof legacy_operations[0]),
                             validate_resource };
    return module;
}
