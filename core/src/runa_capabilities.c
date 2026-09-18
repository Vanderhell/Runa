#include "runa_capabilities.h"

#include "runa_ir.h"

#define RUNA_MODULE_RECORD_HEADER_SIZE 10u

static size_t module_payload(const runa_module_t *module, uint8_t *buffer) {
    if (module->capabilities == NULL) return 0u;
    return module->capabilities(module->context, buffer, RUNA_MAX_MODULE_CAPABILITY_PAYLOAD);
}

runa_status_t runa_capabilities_encode(const runa_module_registry_t *registry,
                                       uint8_t *output, size_t capacity, size_t *written) {
    uint8_t payloads[RUNA_MAX_MODULES][RUNA_MAX_MODULE_CAPABILITY_PAYLOAD];
    uint8_t payload_sizes[RUNA_MAX_MODULES];
    size_t total = RUNA_CAPABILITIES_HEADER_SIZE;
    uint8_t index;
    if (written == NULL) return RUNA_ERR_INVALID_FORMAT;
    *written = 0u;
    if (runa_registry_validate(registry) != RUNA_OK) return RUNA_ERR_INVALID_FORMAT;
    for (index = 0u; index < registry->count; ++index) {
        size_t payload_size = module_payload(registry->modules[index], payloads[index]);
        if (payload_size > RUNA_MAX_MODULE_CAPABILITY_PAYLOAD || payload_size > UINT8_MAX)
            return RUNA_ERR_OUT_OF_RANGE;
        payload_sizes[index] = (uint8_t)payload_size;
        total += RUNA_MODULE_RECORD_HEADER_SIZE + payload_size;
        if (total > RUNA_MAX_CAPABILITY_BYTES || total > UINT16_MAX) return RUNA_ERR_OUT_OF_RANGE;
    }
    if (output == NULL || capacity < total) return RUNA_ERR_OUT_OF_RANGE;
    output[0] = RUNA_CAPABILITIES_FORMAT_VERSION;
    output[1] = RUNA_PROTOCOL_VERSION;
    output[2] = RUNA_IR_VERSION_V2;
    output[3] = RUNA_CAPABILITIES_HEADER_SIZE;
    runa_write_u16_le(output + 4u, (uint16_t)total);
    output[6] = registry->count;
    output[7] = RUNA_REGISTER_COUNT;
    runa_write_u32_le(output + 8u, RUNA_MAX_JOB_BYTES);
    runa_write_u16_le(output + 12u, RUNA_MAX_INSTRUCTIONS);
    runa_write_u16_le(output + 14u, RUNA_MAX_RESULT_BYTES);
    runa_write_u16_le(output + 16u, RUNA_MAX_EMITS);
    runa_write_u16_le(output + 18u, 0u);
    runa_write_u32_le(output + 20u, RUNA_MAX_STEPS);
    runa_write_u32_le(output + 24u, RUNA_MAX_RUNTIME_US);
    runa_write_u32_le(output + 28u, RUNA_MAX_EMIT_BYTES);
    total = RUNA_CAPABILITIES_HEADER_SIZE;
    for (index = 0u; index < registry->count; ++index) {
        const runa_module_t *module = registry->modules[index];
        uint16_t record_size = (uint16_t)(RUNA_MODULE_RECORD_HEADER_SIZE + payload_sizes[index]);
        runa_write_u16_le(output + total, record_size);
        output[total + 2u] = RUNA_CAPABILITY_RECORD_MODULE;
        output[total + 3u] = 0u;
        runa_write_u16_le(output + total + 4u, module->module_id);
        output[total + 6u] = module->abi_version;
        output[total + 7u] = module->module_version;
        runa_write_u16_le(output + total + 8u, payload_sizes[index]);
        if (payload_sizes[index] != 0u)
            for (uint8_t payload_index = 0u; payload_index < payload_sizes[index]; ++payload_index)
                output[total + RUNA_MODULE_RECORD_HEADER_SIZE + payload_index] = payloads[index][payload_index];
        total += record_size;
    }
    *written = total;
    return RUNA_OK;
}

runa_status_t runa_capabilities_decode(const uint8_t *data, size_t size,
                                       runa_capabilities_view_t *view) {
    size_t offset;
    uint8_t record;
    if (data == NULL || view == NULL || size < RUNA_CAPABILITIES_HEADER_SIZE ||
        size > RUNA_MAX_CAPABILITY_BYTES) return RUNA_ERR_INVALID_FORMAT;
    if (data[0] != RUNA_CAPABILITIES_FORMAT_VERSION || data[3] < RUNA_CAPABILITIES_HEADER_SIZE ||
        data[3] > size || runa_read_u16_le(data + 4u) != size)
        return RUNA_ERR_INVALID_FORMAT;
    if (data[7] != RUNA_REGISTER_COUNT) return RUNA_ERR_INVALID_FORMAT;
    view->capability_version = data[0];
    view->protocol_version = data[1];
    view->ir_version = data[2];
    view->module_record_count = data[6];
    view->module_count = 0u;
    view->register_count = data[7];
    view->max_job_bytes = runa_read_u32_le(data + 8u);
    view->max_instructions = runa_read_u16_le(data + 12u);
    view->max_result_bytes = runa_read_u16_le(data + 14u);
    view->max_emits = runa_read_u16_le(data + 16u);
    view->max_steps = runa_read_u32_le(data + 20u);
    view->max_runtime_us = runa_read_u32_le(data + 24u);
    view->max_emit_bytes = runa_read_u32_le(data + 28u);
    offset = data[3];
    for (record = 0u; record < view->module_record_count; ++record) {
        uint16_t record_size;
        if (offset > size || size - offset < 2u) return RUNA_ERR_INVALID_FORMAT;
        record_size = runa_read_u16_le(data + offset);
        if (record_size < 4u || record_size > size - offset) return RUNA_ERR_INVALID_FORMAT;
        if (data[offset + 2u] == RUNA_CAPABILITY_RECORD_MODULE) {
            runa_module_capability_view_t *module;
            uint16_t payload_size;
            uint8_t index;
            if (record_size < RUNA_MODULE_RECORD_HEADER_SIZE ||
                view->module_count >= RUNA_MAX_MODULES) return RUNA_ERR_INVALID_FORMAT;
            payload_size = runa_read_u16_le(data + offset + 8u);
            if (payload_size > record_size - RUNA_MODULE_RECORD_HEADER_SIZE)
                return RUNA_ERR_INVALID_FORMAT;
            module = &view->modules[view->module_count];
            module->module_id = runa_read_u16_le(data + offset + 4u);
            module->abi_version = data[offset + 6u];
            module->module_version = data[offset + 7u];
            module->payload_size = payload_size;
            module->payload = data + offset + RUNA_MODULE_RECORD_HEADER_SIZE;
            for (index = 0u; index < view->module_count; ++index) {
                if (view->modules[index].module_id == module->module_id)
                    return RUNA_ERR_INVALID_FORMAT;
            }
            ++view->module_count;
        }
        offset += record_size;
    }
    if (offset != size) return RUNA_ERR_INVALID_FORMAT;
    return RUNA_OK;
}
