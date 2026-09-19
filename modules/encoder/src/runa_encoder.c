#include "runa_encoder.h"

#include "runa_ir.h"
#include "runa_resource.h"

static runa_status_t validate_resource(void *context, const runa_resource_t *resource,
                                       uint32_t *detail) {
    const runa_encoder_resource_config_t *configuration;
    (void)context;
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (detail != NULL) *detail = resource->id;
    if (resource->resource_type != RUNA_ENCODER_RESOURCE_TYPE) return RUNA_ERR_RESOURCE_TYPE;
    configuration = (const runa_encoder_resource_config_t *)resource->config;
    if (configuration == NULL || configuration->a_pin < 0 || configuration->b_pin < 0 ||
        configuration->a_pin == configuration->b_pin ||
        configuration->decode_mode != RUNA_ENCODER_DECODE_X4 ||
        configuration->invert_direction > 1u || configuration->reserved != 0u)
        return RUNA_ERR_INVALID_RESOURCE;
    return RUNA_OK;
}

static int operation_supported(const runa_encoder_hal_t *hal, uint8_t operation) {
    if (hal == NULL) return 0;
    if (operation == RUNA_ENCODER_OP_READ) return hal->read != NULL;
    if (operation == RUNA_ENCODER_OP_RESET) return hal->reset != NULL;
    if (operation == RUNA_ENCODER_OP_READ_RESET) return hal->read_reset != NULL;
    return 0;
}

static runa_status_t validate(void *context, const runa_module_job_t *job,
                              const runa_module_instruction_t *instruction, uint32_t *detail) {
    const runa_encoder_hal_t *hal = (const runa_encoder_hal_t *)context;
    const runa_resource_t *resource;
    uint16_t resource_id;
    uint8_t register_index = 0u;
    uint32_t permission;
    if (instruction == NULL) return RUNA_ERR_INVALID_OPERAND;
    if (instruction->operation != RUNA_ENCODER_OP_READ &&
        instruction->operation != RUNA_ENCODER_OP_RESET &&
        instruction->operation != RUNA_ENCODER_OP_READ_RESET)
        return RUNA_ERR_INVALID_OPCODE;
    if (instruction->operands == NULL ||
        (instruction->operation == RUNA_ENCODER_OP_RESET ? instruction->operand_size != 2u :
                                                            instruction->operand_size != 3u))
        return RUNA_ERR_INVALID_OPERAND;
    resource_id = runa_read_u16_le(instruction->operands);
    if (instruction->operation != RUNA_ENCODER_OP_RESET) register_index = instruction->operands[2];
    if (detail != NULL) *detail = resource_id;
    if (job == NULL) return RUNA_ERR_INVALID_FORMAT;
    if (instruction->operation != RUNA_ENCODER_OP_RESET && register_index >= job->register_count)
        return RUNA_ERR_INVALID_REGISTER;
    resource = runa_resource_find(job->resources, resource_id);
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (resource->module_id != RUNA_ENCODER_MODULE_ID ||
        resource->resource_type != RUNA_ENCODER_RESOURCE_TYPE)
        return RUNA_ERR_RESOURCE_TYPE;
    permission = instruction->operation == RUNA_ENCODER_OP_READ ? RUNA_PERMISSION_READ :
                  instruction->operation == RUNA_ENCODER_OP_RESET ? RUNA_PERMISSION_WRITE :
                  (RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE);
    if ((resource->permissions & permission) != permission) return RUNA_ERR_ACCESS_DENIED;
    if (validate_resource(NULL, resource, detail) != RUNA_OK) return RUNA_ERR_INVALID_RESOURCE;
    return operation_supported(hal, instruction->operation) != 0 ?
           RUNA_OK : RUNA_ENCODER_ERR_UNSUPPORTED;
}

static runa_status_t map_hal_status(runa_status_t status) {
    if (status == RUNA_OK || status == RUNA_ERR_IO_TIMEOUT || status == RUNA_ERR_INTERNAL ||
        status == RUNA_ERR_OUT_OF_RANGE || status == RUNA_ENCODER_ERR_OVERFLOW)
        return status;
    return RUNA_ENCODER_ERR_IO;
}

static runa_status_t execute(void *context, runa_module_job_t *job,
                             const runa_module_instruction_t *instruction, uint32_t *detail) {
    runa_encoder_hal_t *hal = (runa_encoder_hal_t *)context;
    const runa_resource_t *resource;
    uint16_t resource_id = runa_read_u16_le(instruction->operands);
    uint8_t register_index = instruction->operation == RUNA_ENCODER_OP_RESET ? 0u :
                             instruction->operands[2];
    runa_status_t status;
    if (detail != NULL) *detail = resource_id;
    resource = runa_resource_find(job->resources, resource_id);
    if (resource == NULL || hal == NULL) return RUNA_ERR_INTERNAL;
    if (instruction->operation == RUNA_ENCODER_OP_RESET) {
        if (hal->reset == NULL) return RUNA_ENCODER_ERR_UNSUPPORTED;
        return map_hal_status(hal->reset(hal->context, resource->platform_handle));
    }
    {
        int32_t position = 0;
        if (instruction->operation == RUNA_ENCODER_OP_READ) {
            if (hal->read == NULL) return RUNA_ENCODER_ERR_UNSUPPORTED;
            status = hal->read(hal->context, resource->platform_handle, &position);
        } else {
            if (hal->read_reset == NULL) return RUNA_ENCODER_ERR_UNSUPPORTED;
            status = hal->read_reset(hal->context, resource->platform_handle, &position);
        }
        status = map_hal_status(status);
        if (status != RUNA_OK) return status;
        job->registers[register_index] = (uint32_t)position;
    }
    return RUNA_OK;
}

static size_t capabilities(void *context, uint8_t *output, size_t capacity) {
    const runa_encoder_hal_t *hal = (const runa_encoder_hal_t *)context;
    uint8_t operations = 0u;
    uint8_t features = 0u;
    if (hal != NULL) {
        if (hal->read != NULL) operations |= 1u << (RUNA_ENCODER_OP_READ - 1u);
        if (hal->reset != NULL) operations |= 1u << (RUNA_ENCODER_OP_RESET - 1u);
        if (hal->read_reset != NULL) operations |= 1u << (RUNA_ENCODER_OP_READ_RESET - 1u);
    }
    features = 1u; /* inversion is part of the resource configuration */
    if (hal != NULL && hal->read_reset != NULL) features |= 2u; /* atomic READ_RESET */
    if (output != NULL && capacity >= 5u) {
        output[0] = 1u; /* capability payload version */
        output[1] = operations;
        output[2] = 32u; /* signed position width */
        output[3] = RUNA_ENCODER_DECODE_X4;
        output[4] = features;
    }
    return 5u;
}

runa_module_t runa_encoder_module(runa_encoder_hal_t *hal) {
    runa_module_t module = { RUNA_ENCODER_MODULE_ID, RUNA_MODULE_ABI_VERSION, 1u,
                             validate, execute, NULL, NULL, capabilities, hal,
                             NULL, 0u, validate_resource };
    return module;
}
