#include "runa_validator.h"

#include "runa_registry.h"
#include "runa_resource.h"

static uint8_t core_operand_size(uint8_t opcode) {
    switch (opcode) {
    case RUNA_OP_NOP: return 0u;
    case RUNA_OP_LOAD_CONST: return 5u;
    case RUNA_OP_MOV: case RUNA_OP_NOT: return 2u;
    case RUNA_OP_ADD: case RUNA_OP_SUB: case RUNA_OP_AND: case RUNA_OP_OR: case RUNA_OP_XOR:
    case RUNA_OP_SHL: case RUNA_OP_SHR: case RUNA_OP_CMP_EQ: case RUNA_OP_CMP_NE:
    case RUNA_OP_CMP_LT: case RUNA_OP_CMP_LE: case RUNA_OP_CMP_GT: case RUNA_OP_CMP_GE: return 3u;
    case RUNA_OP_JUMP: return 2u;
    case RUNA_OP_JUMP_IF: case RUNA_OP_JUMP_IF_NOT: return 3u;
    case RUNA_OP_DELAY_MS: return 4u;
    case RUNA_OP_EMIT: case RUNA_OP_RETURN: return 1u;
    default: return UINT8_MAX;
    }
}

static runa_status_t check_core_instruction(const runa_instruction_t *instruction) {
    const uint8_t *operands = instruction->operands;
    uint8_t expected = core_operand_size(instruction->opcode);
    if (instruction->opcode == RUNA_OP_EXT) return RUNA_OK;
    if (expected == UINT8_MAX) return RUNA_ERR_INVALID_OPCODE;
    if (instruction->operand_size != expected) return RUNA_ERR_INVALID_OPERAND;
    switch (instruction->opcode) {
    case RUNA_OP_LOAD_CONST:
        if (operands[0] >= RUNA_REGISTER_COUNT) return RUNA_ERR_INVALID_REGISTER;
        break;
    case RUNA_OP_MOV: case RUNA_OP_NOT:
        if (operands[0] >= RUNA_REGISTER_COUNT || operands[1] >= RUNA_REGISTER_COUNT)
            return RUNA_ERR_INVALID_REGISTER;
        break;
    case RUNA_OP_ADD: case RUNA_OP_SUB: case RUNA_OP_AND: case RUNA_OP_OR: case RUNA_OP_XOR:
    case RUNA_OP_SHL: case RUNA_OP_SHR: case RUNA_OP_CMP_EQ: case RUNA_OP_CMP_NE:
    case RUNA_OP_CMP_LT: case RUNA_OP_CMP_LE: case RUNA_OP_CMP_GT: case RUNA_OP_CMP_GE:
        if (operands[0] >= RUNA_REGISTER_COUNT || operands[1] >= RUNA_REGISTER_COUNT ||
            operands[2] >= RUNA_REGISTER_COUNT) return RUNA_ERR_INVALID_REGISTER;
        break;
    case RUNA_OP_JUMP_IF: case RUNA_OP_JUMP_IF_NOT:
        if (operands[0] >= RUNA_REGISTER_COUNT) return RUNA_ERR_INVALID_REGISTER;
        break;
    case RUNA_OP_DELAY_MS:
        if (runa_read_u32_le(operands) > RUNA_MAX_SINGLE_DELAY_MS) return RUNA_ERR_OUT_OF_RANGE;
        break;
    case RUNA_OP_EMIT:
        if (operands[0] >= RUNA_REGISTER_COUNT) return RUNA_ERR_INVALID_REGISTER;
        break;
    default:
        break;
    }
    return RUNA_OK;
}

static runa_status_t check_module_instruction(const runa_decoded_job_t *decoded,
                                              const runa_module_registry_t *registry,
                                              const runa_resource_table_t *resources,
                                              uint16_t module_id, uint8_t operation,
                                              const uint8_t *operands, uint8_t operand_size,
                                              uint16_t index, uint32_t byte_offset,
                                              uint32_t *detail) {
    runa_module_job_t job = {0};
    runa_module_instruction_t module_instruction;
    uint32_t registers[RUNA_REGISTER_COUNT] = {0u};
    module_instruction.module_id = module_id;
    module_instruction.operation = operation;
    module_instruction.operand_size = operand_size;
    module_instruction.operands = operands;
    module_instruction.instruction_index = index;
    module_instruction.byte_offset = byte_offset;
    job.job_id = decoded->header.job_id;
    job.instruction_count = decoded->header.instruction_count;
    job.max_steps = decoded->header.max_steps;
    job.max_runtime_us = decoded->header.max_runtime_us;
    job.max_result_bytes = decoded->header.max_result_bytes;
    job.max_emits = decoded->header.max_emits;
    job.max_emit_bytes = decoded->header.max_emit_bytes;
    job.registers = registers;
    job.register_count = RUNA_REGISTER_COUNT;
    job.resources = resources;
    return runa_registry_validate_instruction(registry, &job, &module_instruction, detail);
}

static runa_status_t check_extension(const runa_decoded_job_t *decoded,
                                     const runa_module_registry_t *registry,
                                     const runa_resource_table_t *resources,
                                     const runa_instruction_t *instruction,
                                     uint16_t index, uint32_t *detail) {
    const uint8_t *bytes = instruction->operands;
    if (decoded->ir_version != RUNA_IR_VERSION_V2) return RUNA_ERR_INVALID_OPCODE;
    if (instruction->operand_size < 3u) return RUNA_ERR_INVALID_OPERAND;
    return check_module_instruction(decoded, registry, resources,
                                    runa_read_u16_le(bytes), bytes[2], bytes + 3u,
                                    (uint8_t)(instruction->operand_size - 3u), index,
                                    instruction->offset, detail);
}

runa_status_t runa_validate(const uint8_t *data, size_t size,
                          const runa_module_registry_t *registry,
                          const runa_resource_table_t *resources,
                          runa_decoded_job_t *decoded, runa_validation_error_t *error) {
    uint32_t offsets[RUNA_MAX_INSTRUCTIONS];
    uint32_t offset = RUNA_HEADER_SIZE;
    uint16_t index;
    runa_status_t status;
    runa_instruction_t instruction;
    if (decoded == NULL || error == NULL) return RUNA_ERR_INVALID_FORMAT;
    error->code = RUNA_OK;
    error->instruction_index = UINT16_MAX;
    error->detail = 0u;
    status = runa_decode_header(data, size, decoded);
    if (status != RUNA_OK) goto failed;
    status = runa_registry_validate(registry);
    if (status != RUNA_OK) goto failed;
    status = runa_resource_table_validate(resources, registry);
    if (status != RUNA_OK) goto failed;
    for (index = 0u; index < decoded->header.instruction_count; ++index) {
        offsets[index] = offset;
        status = runa_decode_instruction(data, size, offset, &instruction);
        if (status != RUNA_OK) goto failed_at_instruction;
        if (instruction.opcode == RUNA_OP_EXT)
            status = check_extension(decoded, registry, resources, &instruction, index, &error->detail);
        else {
            status = check_core_instruction(&instruction);
            if (status == RUNA_ERR_INVALID_OPCODE && decoded->ir_version == RUNA_IR_VERSION_V1) {
                const runa_module_t *module;
                uint8_t operation;
                status = runa_registry_find_legacy(registry, instruction.opcode, &module, &operation);
                if (status == RUNA_OK) {
                    status = check_module_instruction(decoded, registry, resources,
                                                      module->module_id, operation,
                                                      instruction.operands, instruction.operand_size,
                                                      index, instruction.offset, &error->detail);
                }
            }
        }
        if (status != RUNA_OK) goto failed_at_instruction;
        offset += (uint32_t)(2u + instruction.operand_size);
        continue;
failed_at_instruction:
        error->instruction_index = index;
        goto failed;
    }
    if (offset != decoded->header.total_size) {
        status = RUNA_ERR_INVALID_FORMAT;
        goto failed;
    }
    offset = RUNA_HEADER_SIZE;
    for (index = 0u; index < decoded->header.instruction_count; ++index) {
        uint16_t target;
        (void)runa_decode_instruction(data, size, offset, &instruction);
        if (instruction.opcode == RUNA_OP_JUMP) target = runa_read_u16_le(instruction.operands);
        else if (instruction.opcode == RUNA_OP_JUMP_IF || instruction.opcode == RUNA_OP_JUMP_IF_NOT)
            target = runa_read_u16_le(instruction.operands + 1u);
        else {
            offset += (uint32_t)(2u + instruction.operand_size);
            continue;
        }
        if (target >= decoded->header.instruction_count || offsets[target] >= decoded->header.total_size) {
            status = RUNA_ERR_INVALID_JUMP;
            error->instruction_index = index;
            error->detail = target;
            goto failed;
        }
        offset += (uint32_t)(2u + instruction.operand_size);
    }
    return RUNA_OK;
failed:
    error->code = status;
    return status;
}
