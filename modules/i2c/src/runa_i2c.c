#include "runa_i2c.h"

#include "runa_ir.h"
#include "runa_result.h"
#include "runa_resource.h"

static runa_status_t validate_resource(void *context, const runa_resource_t *resource,
                                       uint32_t *detail) {
    const runa_i2c_resource_config_t *configuration;
    (void)context;
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (detail != NULL) *detail = resource->id;
    if (resource->resource_type != RUNA_I2C_RESOURCE_TYPE) return RUNA_ERR_RESOURCE_TYPE;
    configuration = (const runa_i2c_resource_config_t *)resource->config;
    if (configuration == NULL || configuration->address == 0u || configuration->address > 0x7fu ||
        configuration->maximum_tx_bytes == 0u || configuration->maximum_tx_bytes > RUNA_I2C_MAX_TX_BYTES ||
        configuration->maximum_rx_bytes == 0u || configuration->maximum_rx_bytes > RUNA_I2C_MAX_RX_BYTES ||
        configuration->clock_hz < RUNA_I2C_MIN_CLOCK_HZ || configuration->clock_hz > RUNA_I2C_MAX_CLOCK_HZ ||
        configuration->maximum_timeout_ms == 0u || configuration->maximum_timeout_ms > RUNA_I2C_MAX_TIMEOUT_MS ||
        configuration->reserved != 0u)
        return RUNA_ERR_INVALID_RESOURCE;
    return RUNA_OK;
}

static runa_status_t validate(void *context, const runa_module_job_t *job,
                              const runa_module_instruction_t *instruction, uint32_t *detail) {
    const runa_resource_t *resource;
    const runa_i2c_resource_config_t *configuration;
    const uint8_t *operands;
    uint16_t resource_id;
    uint8_t transmit_size;
    uint8_t receive_size;
    uint16_t timeout_ms;
    uint8_t permission = 0u;
    (void)context;
    if (instruction == NULL || instruction->operation != RUNA_I2C_OP_TRANSFER)
        return RUNA_ERR_INVALID_OPCODE;
    if (instruction->operands == NULL || instruction->operand_size < 6u)
        return RUNA_ERR_INVALID_OPERAND;
    operands = instruction->operands;
    resource_id = runa_read_u16_le(operands);
    transmit_size = operands[2];
    receive_size = operands[3];
    timeout_ms = runa_read_u16_le(operands + 4u);
    if ((size_t)instruction->operand_size != 6u + transmit_size ||
        ((transmit_size == 0u) && (receive_size == 0u)) || timeout_ms == 0u)
        return RUNA_ERR_INVALID_OPERAND;
    if (transmit_size != 0u) permission |= (uint8_t)RUNA_PERMISSION_WRITE;
    if (receive_size != 0u) permission |= (uint8_t)RUNA_PERMISSION_READ;
    if (detail != NULL) *detail = resource_id;
    if (job == NULL) return RUNA_ERR_INVALID_FORMAT;
    resource = runa_resource_find(job->resources, resource_id);
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (resource->module_id != RUNA_I2C_MODULE_ID || resource->resource_type != RUNA_I2C_RESOURCE_TYPE)
        return RUNA_ERR_RESOURCE_TYPE;
    if ((resource->permissions & permission) != permission) return RUNA_ERR_ACCESS_DENIED;
    if (validate_resource(NULL, resource, detail) != RUNA_OK) return RUNA_ERR_INVALID_RESOURCE;
    configuration = (const runa_i2c_resource_config_t *)resource->config;
    if (transmit_size > configuration->maximum_tx_bytes || receive_size > configuration->maximum_rx_bytes ||
        timeout_ms > configuration->maximum_timeout_ms)
        return RUNA_ERR_OUT_OF_RANGE;
    return RUNA_OK;
}

static runa_status_t execute(void *context, runa_module_job_t *job,
                             const runa_module_instruction_t *instruction, uint32_t *detail) {
    runa_i2c_hal_t *hal = (runa_i2c_hal_t *)context;
    const uint8_t *operands = instruction->operands;
    uint16_t resource_id = runa_read_u16_le(operands);
    uint8_t transmit_size = operands[2];
    uint8_t receive_size = operands[3];
    uint16_t timeout_ms = runa_read_u16_le(operands + 4u);
    const runa_resource_t *resource = runa_resource_find(job->resources, resource_id);
    const runa_i2c_resource_config_t *configuration;
    uint8_t receive[RUNA_I2C_MAX_RX_BYTES] = {0u};
    runa_status_t status;
    size_t offset = 0u;
    uint16_t sequence = 0u;
    if (detail != NULL) *detail = resource_id;
    if (resource == NULL || resource->config == NULL || hal == NULL || hal->transfer == NULL)
        return RUNA_ERR_INTERNAL;
    configuration = (const runa_i2c_resource_config_t *)resource->config;
    status = hal->transfer(hal->context, resource->platform_handle, configuration,
                           transmit_size == 0u ? NULL : operands + 6u, transmit_size,
                           receive_size == 0u ? NULL : receive, receive_size, timeout_ms);
    if (status != RUNA_OK) return status;
    while (offset < receive_size) {
        size_t chunk = (size_t)receive_size - offset;
        if (chunk > RUNA_MAX_MODULE_DATA_BYTES) chunk = RUNA_MAX_MODULE_DATA_BYTES;
        if (job->emit_data == NULL) return RUNA_ERR_INTERNAL;
        status = job->emit_data(job->emit_context, RUNA_I2C_MODULE_ID,
                                instruction->instruction_index, sequence,
                                receive + offset, chunk);
        if (status != RUNA_OK) return status;
        offset += chunk;
        ++sequence;
    }
    return RUNA_OK;
}

static size_t capabilities(void *context, uint8_t *output, size_t capacity) {
    (void)context;
    if (output != NULL && capacity >= 8u) {
        output[0] = RUNA_I2C_OP_TRANSFER;
        output[1] = (uint8_t)RUNA_I2C_MAX_TX_BYTES;
        output[2] = (uint8_t)RUNA_I2C_MAX_RX_BYTES;
        output[3] = 7u;
        output[4] = 1u;
        output[5] = 1u;
        runa_write_u16_le(output + 6u, RUNA_I2C_MAX_TIMEOUT_MS);
    }
    return 8u;
}

runa_module_t runa_i2c_module(runa_i2c_hal_t *hal) {
    runa_module_t module = { RUNA_I2C_MODULE_ID, RUNA_MODULE_ABI_VERSION, 1u,
                             validate, execute, NULL, NULL, capabilities, hal,
                             NULL, 0u, validate_resource };
    return module;
}
