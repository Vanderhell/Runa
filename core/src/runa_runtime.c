#include "runa_runtime.h"

#include "runa_ir.h"
#include "runa_validator.h"

#include <string.h>

static int send_event(const runa_event_sink_t *sink, const uint8_t *bytes, size_t size) {
    return sink != NULL && sink->send != NULL && sink->send(sink->context, bytes, size) == 0;
}

typedef struct module_event_context {
    const runa_event_sink_t *sink;
    uint32_t job_id;
    runa_execution_summary_t *summary;
    uint16_t max_emits;
    uint32_t max_emit_bytes;
} module_event_context_t;

static runa_status_t emit_module_data(void *context, uint16_t module_id,
                                      uint16_t instruction_index, uint16_t sequence,
                                      const uint8_t *data, size_t size) {
    module_event_context_t *event_context = (module_event_context_t *)context;
    uint8_t event[RUNA_MAX_EVENT_BYTES];
    size_t encoded_size;
    if (event_context == NULL || size > RUNA_MAX_MODULE_DATA_BYTES || size > UINT16_MAX ||
        (size != 0u && data == NULL)) return RUNA_ERR_INVALID_FORMAT;
    if (event_context->summary->emit_count >= event_context->max_emits ||
        size > event_context->max_emit_bytes - event_context->summary->emit_bytes)
        return RUNA_ERR_EMIT_LIMIT;
    encoded_size = runa_encode_module_data(event, sizeof event, event_context->job_id,
                                          module_id, instruction_index, sequence,
                                          data, (uint16_t)size);
    if (encoded_size == 0u || !send_event(event_context->sink, event, encoded_size))
        return RUNA_ERR_INTERNAL;
    ++event_context->summary->emit_count;
    event_context->summary->emit_bytes += (uint32_t)size;
    return RUNA_OK;
}

static void send_result(runa_execution_summary_t *summary, const runa_event_sink_t *sink,
                        uint32_t job_id, runa_status_t status, uint16_t instruction,
                        uint32_t detail, const uint8_t *payload, uint16_t payload_size) {
    uint8_t event[RUNA_MAX_EVENT_BYTES];
    size_t size = runa_encode_result(event, sizeof event, job_id,
                                    status == RUNA_OK ? RUNA_RESULT_OK : RUNA_RESULT_ERROR,
                                    status, instruction, detail, payload, payload_size);
    if (size != 0u && send_event(sink, event, size)) summary->result_sent = 1u;
    summary->error = status;
}

static runa_status_t finish_modules(const runa_module_t *const *active, uint8_t count,
                                 const runa_module_job_t *job, runa_status_t status) {
    runa_status_t cleanup_status = RUNA_OK;
    while (count != 0u) {
        const runa_module_t *module = active[--count];
        if (module->end != NULL) {
            runa_status_t end_status = module->end(module->context, job, status);
            if (cleanup_status == RUNA_OK && end_status != RUNA_OK) cleanup_status = end_status;
        }
    }
    return cleanup_status;
}

static runa_status_t active_modules(const uint8_t *data, size_t size,
                                  const runa_decoded_job_t *decoded,
                                  const runa_module_registry_t *registry,
                                  const runa_module_t **active, uint8_t *active_count,
                                  uint32_t *offsets) {
    uint8_t selected[RUNA_MAX_MODULES] = {0u};
    uint16_t index;
    uint8_t module_index;
    uint32_t offset = RUNA_HEADER_SIZE;
    *active_count = 0u;
    for (module_index = 0u; module_index < RUNA_MAX_MODULES; ++module_index) active[module_index] = NULL;
    for (index = 0u; index < decoded->header.instruction_count; ++index) {
        runa_instruction_t instruction;
        if (runa_decode_instruction(data, size, offset, &instruction) != RUNA_OK) return RUNA_ERR_INTERNAL;
        offsets[index] = offset;
        if (instruction.opcode == RUNA_OP_EXT) {
            uint16_t module_id = runa_read_u16_le(instruction.operands);
            for (module_index = 0u; module_index < registry->count; ++module_index) {
                if (registry->modules[module_index]->module_id == module_id) {
                    selected[module_index] = 1u;
                    break;
                }
            }
        } else if (decoded->ir_version == RUNA_IR_VERSION_V1) {
            const runa_module_t *legacy_module;
            uint8_t legacy_operation;
            if (runa_registry_find_legacy(registry, instruction.opcode,
                                          &legacy_module, &legacy_operation) == RUNA_OK) {
                for (module_index = 0u; module_index < registry->count; ++module_index) {
                    if (registry->modules[module_index] == legacy_module) {
                        selected[module_index] = 1u;
                        break;
                    }
                }
            }
        }
        offset += (uint32_t)(2u + instruction.operand_size);
    }
    for (index = 0u; index < registry->count; ++index) {
        if (selected[index] != 0u) active[(*active_count)++] = registry->modules[index];
    }
    return RUNA_OK;
}

runa_execution_summary_t runa_process(const uint8_t *data, size_t size,
                                      const runa_resource_table_t *resources,
                                      const runa_module_registry_t *registry,
                                      const runa_platform_t *platform,
                                      const runa_event_sink_t *sink) {
    runa_execution_summary_t summary = { RUNA_OK, 0u, 0u, 0u, 0u, 0u };
    runa_decoded_job_t decoded;
    runa_validation_error_t validation;
    const runa_module_t *active[RUNA_MAX_MODULES];
    uint8_t active_count = 0u;
    uint32_t offsets[RUNA_MAX_INSTRUCTIONS];
    uint32_t registers[RUNA_REGISTER_COUNT] = {0u};
    uint8_t event[RUNA_MAX_EVENT_BYTES];
    uint8_t result_payload[RUNA_MAX_RESULT_BYTES];
    runa_module_job_t module_job;
    module_event_context_t module_events;
    runa_status_t status;
    uint64_t start;
    uint16_t ip = 0u;
    if (sink == NULL || sink->send == NULL) {
        summary.error = RUNA_ERR_INVALID_FORMAT;
        return summary;
    }
    status = runa_validate(data, size, registry, resources, &decoded, &validation);
    if (status != RUNA_OK) {
        uint32_t id = data != NULL && size >= 12u ? runa_read_u32_le(data + 8u) : 0u;
        send_result(&summary, sink, id, status, validation.instruction_index,
                    validation.detail, NULL, 0u);
        return summary;
    }
    if (platform == NULL || platform->time_us == NULL) {
        send_result(&summary, sink, decoded.header.job_id, RUNA_ERR_INTERNAL,
                    UINT16_MAX, 0u, NULL, 0u);
        return summary;
    }
    status = active_modules(data, size, &decoded, registry, active, &active_count, offsets);
    if (status != RUNA_OK) {
        send_result(&summary, sink, decoded.header.job_id, status, UINT16_MAX, 0u, NULL, 0u);
        return summary;
    }
    module_job.job_id = decoded.header.job_id;
    module_job.instruction_count = decoded.header.instruction_count;
    module_job.reserved = 0u;
    module_job.max_steps = decoded.header.max_steps;
    module_job.max_runtime_us = decoded.header.max_runtime_us;
    module_job.max_result_bytes = decoded.header.max_result_bytes;
    module_job.max_emits = decoded.header.max_emits;
    module_job.max_emit_bytes = decoded.header.max_emit_bytes;
    module_job.registers = registers;
    module_job.register_count = RUNA_REGISTER_COUNT;
    module_job.resources = resources;
    module_job.platform = platform;
    module_job.user_data = NULL;
    module_events.sink = sink;
    module_events.job_id = decoded.header.job_id;
    module_events.summary = &summary;
    module_events.max_emits = decoded.header.max_emits;
    module_events.max_emit_bytes = decoded.header.max_emit_bytes;
    module_job.emit_data = emit_module_data;
    module_job.emit_context = &module_events;

    if (!send_event(sink, event, runa_encode_ack(event, sizeof event, decoded.header.job_id))) {
        summary.error = RUNA_ERR_INTERNAL;
        return summary;
    }
    summary.accepted = 1u;
    for (active_count = 0u; active_count < RUNA_MAX_MODULES && active[active_count] != NULL;
         ++active_count) {
        if (active[active_count]->begin != NULL) {
            status = active[active_count]->begin(active[active_count]->context, &module_job);
            if (status != RUNA_OK) {
                uint8_t begun = (uint8_t)(active_count + 1u);
                (void)finish_modules(active, begun, &module_job, status);
                send_result(&summary, sink, decoded.header.job_id, status, UINT16_MAX, 0u, NULL, 0u);
                return summary;
            }
        }
    }
    active_count = 0u;
    while (active_count < RUNA_MAX_MODULES && active[active_count] != NULL) ++active_count;
    start = platform->time_us(platform->context);
    while (ip < decoded.header.instruction_count) {
        runa_instruction_t instruction;
        const uint8_t *operands;
        uint16_t next = (uint16_t)(ip + 1u);
        uint32_t detail = 0u;
        if (summary.steps >= decoded.header.max_steps) {
            status = RUNA_ERR_STEP_LIMIT;
            detail = summary.steps;
            goto failed;
        }
        if (platform->time_us(platform->context) - start > decoded.header.max_runtime_us) {
            status = RUNA_ERR_RUNTIME_LIMIT;
            goto failed;
        }
        (void)runa_decode_instruction(data, size, offsets[ip], &instruction);
        operands = instruction.operands;
        ++summary.steps;
        switch (instruction.opcode) {
        case RUNA_OP_NOP: break;
        case RUNA_OP_LOAD_CONST: registers[operands[0]] = runa_read_u32_le(operands + 1u); break;
        case RUNA_OP_MOV: registers[operands[0]] = registers[operands[1]]; break;
        case RUNA_OP_ADD: registers[operands[0]] = registers[operands[1]] + registers[operands[2]]; break;
        case RUNA_OP_SUB: registers[operands[0]] = registers[operands[1]] - registers[operands[2]]; break;
        case RUNA_OP_AND: registers[operands[0]] = registers[operands[1]] & registers[operands[2]]; break;
        case RUNA_OP_OR: registers[operands[0]] = registers[operands[1]] | registers[operands[2]]; break;
        case RUNA_OP_XOR: registers[operands[0]] = registers[operands[1]] ^ registers[operands[2]]; break;
        case RUNA_OP_NOT: registers[operands[0]] = ~registers[operands[1]]; break;
        case RUNA_OP_SHL: registers[operands[0]] = registers[operands[1]] << (registers[operands[2]] & 31u); break;
        case RUNA_OP_SHR: registers[operands[0]] = registers[operands[1]] >> (registers[operands[2]] & 31u); break;
        case RUNA_OP_CMP_EQ: registers[operands[0]] = registers[operands[1]] == registers[operands[2]] ? 1u : 0u; break;
        case RUNA_OP_CMP_NE: registers[operands[0]] = registers[operands[1]] != registers[operands[2]] ? 1u : 0u; break;
        case RUNA_OP_CMP_LT: registers[operands[0]] = registers[operands[1]] < registers[operands[2]] ? 1u : 0u; break;
        case RUNA_OP_CMP_LE: registers[operands[0]] = registers[operands[1]] <= registers[operands[2]] ? 1u : 0u; break;
        case RUNA_OP_CMP_GT: registers[operands[0]] = registers[operands[1]] > registers[operands[2]] ? 1u : 0u; break;
        case RUNA_OP_CMP_GE: registers[operands[0]] = registers[operands[1]] >= registers[operands[2]] ? 1u : 0u; break;
        case RUNA_OP_JUMP: next = runa_read_u16_le(operands); break;
        case RUNA_OP_JUMP_IF: if (registers[operands[0]] != 0u) next = runa_read_u16_le(operands + 1u); break;
        case RUNA_OP_JUMP_IF_NOT: if (registers[operands[0]] == 0u) next = runa_read_u16_le(operands + 1u); break;
        case RUNA_OP_DELAY_MS:
            if (platform->delay_ms == NULL) { status = RUNA_ERR_INTERNAL; goto failed; }
            status = platform->delay_ms(platform->context, runa_read_u32_le(operands));
            if (status != RUNA_OK) goto failed;
            break;
        case RUNA_OP_EMIT:
            if (summary.emit_count >= decoded.header.max_emits ||
                summary.emit_bytes + 4u > decoded.header.max_emit_bytes) {
                status = RUNA_ERR_EMIT_LIMIT;
                detail = summary.emit_count;
                goto failed;
            }
            if (!send_event(sink, event, runa_encode_emit(event, sizeof event,
                            decoded.header.job_id, ip, registers[operands[0]]))) {
                status = RUNA_ERR_INTERNAL;
                goto failed;
            }
            ++summary.emit_count;
            summary.emit_bytes += 4u;
            break;
        case RUNA_OP_RETURN: {
            uint8_t mask = operands[0];
            uint16_t payload_size = 0u;
            uint8_t reg;
            for (reg = 0u; reg < RUNA_REGISTER_COUNT; ++reg) {
                if ((mask & (uint8_t)(1u << reg)) != 0u) {
                    if ((uint16_t)(payload_size + 4u) > decoded.header.max_result_bytes) {
                        status = RUNA_ERR_RESULT_LIMIT;
                        detail = (uint32_t)payload_size + 4u;
                        goto failed;
                    }
                    runa_write_u32_le(result_payload + payload_size, registers[reg]);
                    payload_size = (uint16_t)(payload_size + 4u);
                }
            }
            status = finish_modules(active, active_count, &module_job, RUNA_OK);
            if (status != RUNA_OK) {
                send_result(&summary, sink, decoded.header.job_id, status, ip, 0u, NULL, 0u);
                return summary;
            }
            send_result(&summary, sink, decoded.header.job_id, RUNA_OK, ip, 0u,
                        result_payload, payload_size);
            return summary;
        }
        case RUNA_OP_EXT: {
            runa_module_instruction_t module_instruction;
            const runa_module_t *module;
            module_instruction.module_id = runa_read_u16_le(operands);
            module_instruction.operation = operands[2];
            module_instruction.operand_size = (uint8_t)(instruction.operand_size - 3u);
            module_instruction.operands = operands + 3u;
            module_instruction.instruction_index = ip;
            module_instruction.byte_offset = instruction.offset;
            module = runa_registry_find(registry, module_instruction.module_id);
            status = module->execute(module->context, &module_job, &module_instruction, &detail);
            if (status != RUNA_OK) goto failed;
            break;
        }
        default: {
            runa_module_instruction_t module_instruction;
            const runa_module_t *module;
            uint8_t operation;
            if (decoded.ir_version != RUNA_IR_VERSION_V1 ||
                runa_registry_find_legacy(registry, instruction.opcode, &module, &operation) != RUNA_OK) {
                status = RUNA_ERR_INTERNAL;
                goto failed;
            }
            module_instruction.module_id = module->module_id;
            module_instruction.operation = operation;
            module_instruction.operand_size = instruction.operand_size;
            module_instruction.operands = operands;
            module_instruction.instruction_index = ip;
            module_instruction.byte_offset = instruction.offset;
            status = module->execute(module->context, &module_job, &module_instruction, &detail);
            if (status != RUNA_OK) goto failed;
            break;
        }
        }
        if (platform->time_us(platform->context) - start > decoded.header.max_runtime_us) {
            status = RUNA_ERR_RUNTIME_LIMIT;
            detail = 0u;
            goto failed;
        }
        ip = next;
        continue;
failed:
        (void)finish_modules(active, active_count, &module_job, status);
        send_result(&summary, sink, decoded.header.job_id, status, ip, detail, NULL, 0u);
        return summary;
    }
    (void)finish_modules(active, active_count, &module_job, RUNA_ERR_INVALID_FORMAT);
    send_result(&summary, sink, decoded.header.job_id, RUNA_ERR_INVALID_FORMAT, ip, 0u, NULL, 0u);
    return summary;
}
