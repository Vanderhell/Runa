#include "runa_ir.h"

uint16_t runa_read_u16_le(const uint8_t *data) {
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8u));
}

uint32_t runa_read_u32_le(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8u) |
           ((uint32_t)data[2] << 16u) | ((uint32_t)data[3] << 24u);
}

void runa_write_u16_le(uint8_t *data, uint16_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
}

void runa_write_u32_le(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

runa_status_t runa_decode_header(const uint8_t *data, size_t size, runa_decoded_job_t *decoded) {
    runa_header_t *header;
    if (data == NULL || decoded == NULL) return RUNA_ERR_INVALID_FORMAT;
    if (size > RUNA_MAX_JOB_BYTES) return RUNA_ERR_JOB_TOO_LARGE;
    if (size < RUNA_HEADER_SIZE) return RUNA_ERR_INVALID_FORMAT;
    if (data[0] != (uint8_t)'J' || data[1] != (uint8_t)'E' ||
        data[2] != (uint8_t)'X' || data[3] != (uint8_t)'E') return RUNA_ERR_BAD_MAGIC;
    if (data[4] != RUNA_PROTOCOL_VERSION) return RUNA_ERR_UNSUPPORTED_PROTOCOL;
    if (data[5] != RUNA_IR_VERSION_V1 && data[5] != RUNA_IR_VERSION_V2)
        return RUNA_ERR_UNSUPPORTED_IR_VERSION;
    if (runa_read_u16_le(data + 6u) != RUNA_HEADER_SIZE || runa_read_u16_le(data + 22u) != 0u)
        return RUNA_ERR_INVALID_FORMAT;
    header = &decoded->header;
    header->job_id = runa_read_u32_le(data + 8u);
    header->total_size = runa_read_u32_le(data + 12u);
    header->instruction_bytes = runa_read_u32_le(data + 16u);
    header->instruction_count = runa_read_u16_le(data + 20u);
    header->max_steps = runa_read_u32_le(data + 24u);
    header->max_runtime_us = runa_read_u32_le(data + 28u);
    header->max_result_bytes = runa_read_u16_le(data + 32u);
    header->max_emits = runa_read_u16_le(data + 34u);
    header->max_emit_bytes = runa_read_u32_le(data + 36u);
    decoded->ir_version = data[5];
    if (header->total_size != size ||
        header->instruction_bytes > RUNA_MAX_JOB_BYTES - RUNA_HEADER_SIZE ||
        header->total_size != RUNA_HEADER_SIZE + header->instruction_bytes) return RUNA_ERR_INVALID_FORMAT;
    if (header->instruction_count == 0u || header->instruction_count > RUNA_MAX_INSTRUCTIONS)
        return RUNA_ERR_INVALID_FORMAT;
    if (header->max_steps == 0u || header->max_steps > RUNA_MAX_STEPS ||
        header->max_runtime_us == 0u || header->max_runtime_us > RUNA_MAX_RUNTIME_US ||
        header->max_result_bytes > RUNA_MAX_RESULT_BYTES || header->max_emits > RUNA_MAX_EMITS ||
        header->max_emit_bytes > RUNA_MAX_EMIT_BYTES) return RUNA_ERR_OUT_OF_RANGE;
    return RUNA_OK;
}

runa_status_t runa_decode_instruction(const uint8_t *data, size_t size, uint32_t offset,
                                      runa_instruction_t *instruction) {
    size_t position = (size_t)offset;
    if (data == NULL || instruction == NULL || position > size || size - position < 2u)
        return RUNA_ERR_INVALID_FORMAT;
    instruction->opcode = data[position];
    instruction->operand_size = data[position + 1u];
    if ((size_t)instruction->operand_size > size - position - 2u) return RUNA_ERR_INVALID_FORMAT;
    instruction->operands = data + position + 2u;
    instruction->offset = offset;
    return RUNA_OK;
}
