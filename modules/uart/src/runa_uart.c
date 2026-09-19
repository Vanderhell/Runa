#include "runa_uart.h"

#include "runa_ir.h"
#include "runa_result.h"
#include "runa_resource.h"

static runa_status_t validate_resource(void *context, const runa_resource_t *resource,
                                       uint32_t *detail) {
    const runa_uart_resource_config_t *configuration;
    (void)context;
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (detail != NULL) *detail = resource->id;
    if (resource->resource_type != RUNA_UART_RESOURCE_TYPE) return RUNA_ERR_RESOURCE_TYPE;
    configuration = (const runa_uart_resource_config_t *)resource->config;
    if (configuration == NULL || configuration->controller >= 3u || configuration->data_bits < 5u ||
        configuration->data_bits > 8u || configuration->stop_bits < 1u || configuration->stop_bits > 2u ||
        configuration->parity > RUNA_UART_PARITY_ODD || configuration->flow_control > RUNA_UART_FLOW_RTS_CTS ||
        configuration->reserved0 != 0u || configuration->tx_pin < 0 || configuration->rx_pin < 0 ||
        configuration->baud_rate < RUNA_UART_MIN_BAUD || configuration->baud_rate > RUNA_UART_MAX_BAUD ||
        configuration->maximum_tx_bytes == 0u || configuration->maximum_tx_bytes > RUNA_UART_MAX_TX_BYTES ||
        configuration->maximum_rx_bytes == 0u || configuration->maximum_rx_bytes > RUNA_UART_MAX_RX_BYTES ||
        configuration->maximum_timeout_us == 0u || configuration->maximum_timeout_us > RUNA_UART_MAX_TIMEOUT_US ||
        configuration->reserved1 != 0u)
        return RUNA_ERR_INVALID_RESOURCE;
    return RUNA_OK;
}

static runa_status_t validate(void *context, const runa_module_job_t *job,
                              const runa_module_instruction_t *instruction, uint32_t *detail) {
    const runa_resource_t *resource;
    const runa_uart_resource_config_t *configuration;
    const uint8_t *operands;
    uint16_t resource_id;
    uint8_t transmit_size;
    uint8_t receive_size;
    uint8_t policy;
    uint32_t timeout_us;
    uint8_t permission = 0u;
    (void)context;
    if (instruction == NULL || instruction->operation != RUNA_UART_OP_TRANSFER)
        return RUNA_ERR_INVALID_OPCODE;
    if (instruction->operands == NULL || instruction->operand_size < 9u)
        return RUNA_ERR_INVALID_OPERAND;
    operands = instruction->operands;
    resource_id = runa_read_u16_le(operands);
    transmit_size = operands[2];
    receive_size = operands[3];
    policy = operands[4];
    timeout_us = runa_read_u32_le(operands + 5u);
    if ((size_t)instruction->operand_size != 9u + transmit_size ||
        ((transmit_size == 0u) && (receive_size == 0u)) || policy > RUNA_UART_RX_UP_TO_LENGTH ||
        (receive_size != 0u && timeout_us == 0u) ||
        (receive_size == 0u && policy != RUNA_UART_RX_FIXED_LENGTH))
        return RUNA_ERR_INVALID_OPERAND;
    if (transmit_size != 0u) permission |= (uint8_t)RUNA_PERMISSION_WRITE;
    if (receive_size != 0u) permission |= (uint8_t)RUNA_PERMISSION_READ;
    if (detail != NULL) *detail = resource_id;
    if (job == NULL) return RUNA_ERR_INVALID_FORMAT;
    if (timeout_us > job->max_runtime_us) return RUNA_ERR_OUT_OF_RANGE;
    resource = runa_resource_find(job->resources, resource_id);
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (resource->module_id != RUNA_UART_MODULE_ID || resource->resource_type != RUNA_UART_RESOURCE_TYPE)
        return RUNA_ERR_RESOURCE_TYPE;
    if ((resource->permissions & permission) != permission) return RUNA_ERR_ACCESS_DENIED;
    if (validate_resource(NULL, resource, detail) != RUNA_OK) return RUNA_ERR_INVALID_RESOURCE;
    configuration = (const runa_uart_resource_config_t *)resource->config;
    if (transmit_size > configuration->maximum_tx_bytes || receive_size > configuration->maximum_rx_bytes ||
        timeout_us > configuration->maximum_timeout_us)
        return RUNA_ERR_OUT_OF_RANGE;
    return RUNA_OK;
}

static runa_status_t execute(void *context, runa_module_job_t *job,
                             const runa_module_instruction_t *instruction, uint32_t *detail) {
    runa_uart_hal_t *hal = (runa_uart_hal_t *)context;
    const uint8_t *operands = instruction->operands;
    uint16_t resource_id = runa_read_u16_le(operands);
    uint8_t transmit_size = operands[2];
    uint8_t receive_size = operands[3];
    uint8_t policy = operands[4];
    uint32_t timeout_us = runa_read_u32_le(operands + 5u);
    const runa_resource_t *resource = runa_resource_find(job->resources, resource_id);
    const runa_uart_resource_config_t *configuration;
    uint8_t receive[RUNA_UART_MAX_RX_BYTES] = {0u};
    size_t actual_receive = 0u;
    size_t offset = 0u;
    uint16_t sequence = 0u;
    runa_status_t status;
    if (detail != NULL) *detail = resource_id;
    if (resource == NULL || resource->config == NULL || hal == NULL || hal->transfer == NULL)
        return RUNA_ERR_INTERNAL;
    configuration = (const runa_uart_resource_config_t *)resource->config;
    status = hal->transfer(hal->context, resource->platform_handle, configuration,
                           transmit_size == 0u ? NULL : operands + 9u, transmit_size,
                           receive_size == 0u ? NULL : receive, receive_size, &actual_receive,
                           policy, timeout_us);
    if (status != RUNA_OK) return status;
    if (actual_receive > receive_size || (policy == RUNA_UART_RX_FIXED_LENGTH && actual_receive != receive_size))
        return policy == RUNA_UART_RX_FIXED_LENGTH ? RUNA_UART_ERR_PARTIAL : RUNA_ERR_INTERNAL;
    while (offset < actual_receive) {
        size_t chunk = actual_receive - offset;
        if (chunk > RUNA_MAX_MODULE_DATA_BYTES) chunk = RUNA_MAX_MODULE_DATA_BYTES;
        if (job->emit_data == NULL) return RUNA_ERR_INTERNAL;
        status = job->emit_data(job->emit_context, RUNA_UART_MODULE_ID,
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
    if (output != NULL && capacity >= 12u) {
        output[0] = RUNA_UART_OP_TRANSFER;
        output[1] = (uint8_t)RUNA_UART_MAX_TX_BYTES;
        output[2] = (uint8_t)RUNA_UART_MAX_RX_BYTES;
        output[3] = (uint8_t)((1u << RUNA_UART_RX_FIXED_LENGTH) | (1u << RUNA_UART_RX_UP_TO_LENGTH));
        output[4] = 8u;
        output[5] = 3u;
        output[6] = 2u;
        output[7] = RUNA_UART_FLOW_NONE;
        runa_write_u32_le(output + 8u, RUNA_UART_MAX_TIMEOUT_US);
    }
    return 12u;
}

runa_module_t runa_uart_module(runa_uart_hal_t *hal) {
    runa_module_t module = { RUNA_UART_MODULE_ID, RUNA_MODULE_ABI_VERSION, 1u,
                             validate, execute, NULL, NULL, capabilities, hal,
                             NULL, 0u, validate_resource };
    return module;
}
