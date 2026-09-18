#include "runa_result.h"

#include "runa_ir.h"

static size_t frame_prefix(uint8_t *output, size_t capacity, uint8_t type,
                           uint16_t size, uint32_t job_id) {
    if (output == NULL || capacity < size) return 0u;
    output[0] = type;
    output[1] = RUNA_PROTOCOL_VERSION;
    runa_write_u16_le(output + 2u, size);
    runa_write_u32_le(output + 4u, job_id);
    return size;
}

size_t runa_encode_ack(uint8_t *output, size_t capacity, uint32_t job_id) {
    return frame_prefix(output, capacity, RUNA_EVENT_ACK, 8u, job_id);
}

size_t runa_encode_emit(uint8_t *output, size_t capacity, uint32_t job_id,
                        uint16_t instruction, uint32_t value) {
    size_t size = frame_prefix(output, capacity, RUNA_EVENT_EMIT, 14u, job_id);
    if (size != 0u) {
        runa_write_u16_le(output + 8u, instruction);
        runa_write_u32_le(output + 10u, value);
    }
    return size;
}

size_t runa_encode_result(uint8_t *output, size_t capacity, uint32_t job_id,
                          runa_result_status_t result, runa_status_t status,
                          uint16_t instruction, uint32_t detail,
                          const uint8_t *payload, uint16_t payload_size) {
    uint16_t size;
    uint16_t index;
    if (payload_size > RUNA_MAX_RESULT_BYTES || (payload_size != 0u && payload == NULL)) return 0u;
    size = (uint16_t)(20u + payload_size);
    if (frame_prefix(output, capacity, RUNA_EVENT_RESULT, size, job_id) == 0u) return 0u;
    output[8] = (uint8_t)result;
    output[9] = status;
    runa_write_u16_le(output + 10u, instruction);
    runa_write_u32_le(output + 12u, detail);
    runa_write_u16_le(output + 16u, payload_size);
    runa_write_u16_le(output + 18u, 0u);
    for (index = 0u; index < payload_size; ++index) output[20u + index] = payload[index];
    return size;
}

size_t runa_encode_module_data(uint8_t *output, size_t capacity, uint32_t job_id,
                               uint16_t module_id, uint16_t instruction,
                               uint16_t sequence, const uint8_t *payload,
                               uint16_t payload_size) {
    uint16_t size;
    uint16_t index;
    if (module_id == 0u || payload_size > RUNA_MAX_MODULE_DATA_BYTES ||
        (payload_size != 0u && payload == NULL)) return 0u;
    size = (uint16_t)(16u + payload_size);
    if (frame_prefix(output, capacity, RUNA_EVENT_MODULE_DATA, size, job_id) == 0u) return 0u;
    runa_write_u16_le(output + 8u, module_id);
    runa_write_u16_le(output + 10u, instruction);
    runa_write_u16_le(output + 12u, sequence);
    runa_write_u16_le(output + 14u, payload_size);
    for (index = 0u; index < payload_size; ++index) output[16u + index] = payload[index];
    return size;
}
