#include "runa_can.h"

#include "runa_ir.h"
#include "runa_result.h"
#include "runa_resource.h"

#include <string.h>

static runa_status_t validate_frame(const runa_can_frame_t *frame) {
    if (frame == NULL || (frame->flags & (uint8_t)~RUNA_CAN_FRAME_EXTENDED) != 0u ||
        frame->length > RUNA_CAN_MAX_PAYLOAD_BYTES ||
        ((frame->flags & RUNA_CAN_FRAME_EXTENDED) == 0u && frame->id > 0x7ffu) ||
        ((frame->flags & RUNA_CAN_FRAME_EXTENDED) != 0u && frame->id > 0x1fffffffu))
        return RUNA_ERR_INVALID_OPERAND;
    return RUNA_OK;
}

static runa_status_t validate_resource(void *context, const runa_resource_t *resource,
                                       uint32_t *detail) {
    const runa_can_resource_config_t *configuration;
    uint32_t id_limit;
    (void)context;
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (detail != NULL) *detail = resource->id;
    if (resource->resource_type != RUNA_CAN_RESOURCE_TYPE) return RUNA_ERR_RESOURCE_TYPE;
    configuration = (const runa_can_resource_config_t *)resource->config;
    if (configuration == NULL || configuration->controller >= 2u || configuration->filter_mode > RUNA_CAN_FILTER_EXTENDED ||
        configuration->listen_only > 1u || configuration->reserved0 != 0u ||
        configuration->bitrate < RUNA_CAN_MIN_BITRATE || configuration->bitrate > RUNA_CAN_MAX_BITRATE ||
        configuration->maximum_timeout_us == 0u || configuration->maximum_timeout_us > RUNA_CAN_MAX_TIMEOUT_US ||
        configuration->reserved1 != 0u)
        return RUNA_ERR_INVALID_RESOURCE;
    id_limit = configuration->filter_mode == RUNA_CAN_FILTER_STANDARD ? 0x7ffu : 0x1fffffffu;
    if (configuration->accept_id > id_limit || configuration->accept_mask > id_limit)
        return RUNA_ERR_INVALID_RESOURCE;
    return RUNA_OK;
}

static runa_status_t validate_filter(const runa_can_resource_config_t *configuration,
                                     const runa_can_frame_t *frame) {
    uint8_t extended = (uint8_t)((frame->flags & RUNA_CAN_FRAME_EXTENDED) != 0u);
    if (extended != configuration->filter_mode) return RUNA_CAN_ERR_FILTER;
    if ((frame->id & configuration->accept_mask) != (configuration->accept_id & configuration->accept_mask))
        return RUNA_CAN_ERR_FILTER;
    return RUNA_OK;
}

static runa_status_t decode_frame(const uint8_t *operands, runa_can_frame_t *frame) {
    frame->flags = operands[0];
    frame->id = runa_read_u32_le(operands + 1u);
    frame->length = operands[5];
    if (validate_frame(frame) != RUNA_OK) return RUNA_ERR_INVALID_OPERAND;
    memcpy(frame->data, operands + 6u, frame->length);
    return RUNA_OK;
}

static runa_status_t validate(void *context, const runa_module_job_t *job,
                              const runa_module_instruction_t *instruction, uint32_t *detail) {
    const runa_resource_t *resource;
    const runa_can_resource_config_t *configuration;
    const uint8_t *operands;
    uint16_t resource_id;
    uint32_t timeout_us;
    uint8_t permission;
    size_t expected;
    runa_can_frame_t frame;
    (void)context;
    if (instruction == NULL) return RUNA_ERR_INVALID_OPCODE;
    if (instruction->operands == NULL) return RUNA_ERR_INVALID_OPERAND;
    operands = instruction->operands;
    if (instruction->operation == RUNA_CAN_OP_TRANSMIT || instruction->operation == RUNA_CAN_OP_REQUEST_RESPONSE) {
        size_t frame_header = 8u;
        if (instruction->operand_size < frame_header || decode_frame(operands + 2u, &frame) != RUNA_OK)
            return RUNA_ERR_INVALID_OPERAND;
        expected = 12u + frame.length;
        if ((size_t)instruction->operand_size != expected)
            return RUNA_ERR_INVALID_OPERAND;
        timeout_us = runa_read_u32_le(operands + 8u + frame.length);
        permission = instruction->operation == RUNA_CAN_OP_TRANSMIT ? (uint8_t)RUNA_PERMISSION_WRITE :
                     (uint8_t)(RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE);
    } else if (instruction->operation == RUNA_CAN_OP_RECEIVE) {
        if (instruction->operand_size != 6u) return RUNA_ERR_INVALID_OPERAND;
        resource_id = runa_read_u16_le(operands);
        timeout_us = runa_read_u32_le(operands + 2u);
        permission = (uint8_t)RUNA_PERMISSION_READ;
        if (timeout_us == 0u) return RUNA_ERR_INVALID_OPERAND;
    } else return RUNA_ERR_INVALID_OPCODE;
    resource_id = runa_read_u16_le(operands);
    if (detail != NULL) *detail = resource_id;
    if (job == NULL) return RUNA_ERR_INVALID_FORMAT;
    if (timeout_us == 0u) return RUNA_ERR_INVALID_OPERAND;
    if (timeout_us > job->max_runtime_us) return RUNA_ERR_OUT_OF_RANGE;
    resource = runa_resource_find(job->resources, resource_id);
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (resource->module_id != RUNA_CAN_MODULE_ID || resource->resource_type != RUNA_CAN_RESOURCE_TYPE)
        return RUNA_ERR_RESOURCE_TYPE;
    if ((resource->permissions & permission) != permission) return RUNA_ERR_ACCESS_DENIED;
    if (validate_resource(NULL, resource, detail) != RUNA_OK) return RUNA_ERR_INVALID_RESOURCE;
    configuration = (const runa_can_resource_config_t *)resource->config;
    if (timeout_us > configuration->maximum_timeout_us) return RUNA_ERR_OUT_OF_RANGE;
    return RUNA_OK;
}

static runa_status_t emit_frame(runa_module_job_t *job, const runa_module_instruction_t *instruction,
                                const runa_can_frame_t *frame) {
    uint8_t encoded[14] = {0u};
    encoded[0] = frame->flags;
    runa_write_u32_le(encoded + 1u, frame->id);
    encoded[5] = frame->length;
    memcpy(encoded + 6u, frame->data, frame->length);
    return job->emit_data == NULL ? RUNA_ERR_INTERNAL :
           job->emit_data(job->emit_context, RUNA_CAN_MODULE_ID, instruction->instruction_index,
                          0u, encoded, (size_t)6u + frame->length);
}

static runa_status_t execute(void *context, runa_module_job_t *job,
                             const runa_module_instruction_t *instruction, uint32_t *detail) {
    runa_can_hal_t *hal = (runa_can_hal_t *)context;
    const uint8_t *operands = instruction->operands;
    uint16_t resource_id = runa_read_u16_le(operands);
    const runa_resource_t *resource = runa_resource_find(job->resources, resource_id);
    const runa_can_resource_config_t *configuration;
    runa_can_frame_t frame = {0};
    runa_status_t status;
    uint32_t timeout_us;
    if (detail != NULL) *detail = resource_id;
    if (resource == NULL || resource->config == NULL || hal == NULL ||
        ((instruction->operation == RUNA_CAN_OP_RECEIVE) && hal->receive == NULL) ||
        ((instruction->operation != RUNA_CAN_OP_RECEIVE) && hal->transmit == NULL) ||
        ((instruction->operation == RUNA_CAN_OP_REQUEST_RESPONSE) && hal->receive == NULL)) return RUNA_ERR_INTERNAL;
    configuration = (const runa_can_resource_config_t *)resource->config;
    if (instruction->operation == RUNA_CAN_OP_TRANSMIT || instruction->operation == RUNA_CAN_OP_REQUEST_RESPONSE) {
        status = decode_frame(operands + 2u, &frame);
        if (status != RUNA_OK) return status;
        timeout_us = runa_read_u32_le(operands + 8u + frame.length);
        status = hal->transmit(hal->context, resource->platform_handle, &frame, timeout_us);
        if (status != RUNA_OK || instruction->operation == RUNA_CAN_OP_TRANSMIT) return status;
    }
    timeout_us = instruction->operation == RUNA_CAN_OP_RECEIVE ? runa_read_u32_le(operands + 2u) :
                 runa_read_u32_le(operands + 8u + frame.length);
    status = hal->receive(hal->context, resource->platform_handle, &frame, timeout_us);
    if (status != RUNA_OK) return status;
    if (validate_frame(&frame) != RUNA_OK || validate_filter(configuration, &frame) != RUNA_OK)
        return RUNA_CAN_ERR_FILTER;
    return emit_frame(job, instruction, &frame);
}

static size_t capabilities(void *context, uint8_t *output, size_t capacity) {
    (void)context;
    if (output != NULL && capacity >= 12u) {
        output[0] = RUNA_CAN_OP_TRANSMIT; output[1] = RUNA_CAN_OP_RECEIVE; output[2] = RUNA_CAN_OP_REQUEST_RESPONSE;
        output[3] = 0x03u; output[4] = RUNA_CAN_MAX_PAYLOAD_BYTES; output[5] = 0u;
        runa_write_u32_le(output + 6u, RUNA_CAN_MAX_TIMEOUT_US);
        output[10] = RUNA_CAN_FILTER_STANDARD; output[11] = RUNA_CAN_FILTER_EXTENDED;
    }
    return 12u;
}

runa_module_t runa_can_module(runa_can_hal_t *hal) {
    runa_module_t module = { RUNA_CAN_MODULE_ID, RUNA_MODULE_ABI_VERSION, 1u,
                             validate, execute, NULL, NULL, capabilities, hal,
                             NULL, 0u, validate_resource };
    return module;
}
