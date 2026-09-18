#include "runa_registry.h"

#include <stddef.h>

void runa_registry_init(runa_module_registry_t *registry) {
    uint8_t index;
    if (registry == NULL) return;
    registry->count = 0u;
    for (index = 0u; index < RUNA_MAX_MODULES; ++index) registry->modules[index] = NULL;
}

runa_status_t runa_registry_add(runa_module_registry_t *registry, const runa_module_t *module) {
    uint8_t index;
    uint8_t legacy_index;
    if (registry == NULL || module == NULL || module->module_id == 0u ||
        module->abi_version != RUNA_MODULE_ABI_VERSION || module->validate == NULL ||
        module->execute == NULL || module->validate_resource == NULL ||
        (module->legacy_operation_count != 0u && module->legacy_operations == NULL))
        return RUNA_ERR_INVALID_FORMAT;
    if (registry->count > RUNA_MAX_MODULES || runa_registry_validate(registry) != RUNA_OK)
        return RUNA_ERR_INVALID_FORMAT;
    for (index = 0u; index < registry->count; ++index) {
        uint8_t existing_legacy;
        const runa_module_t *existing = registry->modules[index];
        if (existing->module_id == module->module_id) return RUNA_ERR_INVALID_FORMAT;
        for (legacy_index = 0u; legacy_index < module->legacy_operation_count; ++legacy_index) {
            for (existing_legacy = 0u; existing_legacy < existing->legacy_operation_count;
                 ++existing_legacy) {
                if (module->legacy_operations[legacy_index].opcode ==
                    existing->legacy_operations[existing_legacy].opcode) return RUNA_ERR_INVALID_FORMAT;
            }
        }
    }
    for (legacy_index = 0u; legacy_index < module->legacy_operation_count; ++legacy_index) {
        uint8_t previous;
        for (previous = 0u; previous < legacy_index; ++previous) {
            if (module->legacy_operations[legacy_index].opcode ==
                module->legacy_operations[previous].opcode) return RUNA_ERR_INVALID_FORMAT;
        }
    }
    if (registry->count == RUNA_MAX_MODULES) return RUNA_ERR_OUT_OF_RANGE;
    registry->modules[registry->count++] = module;
    return RUNA_OK;
}

const runa_module_t *runa_registry_find(const runa_module_registry_t *registry, uint16_t module_id) {
    uint8_t index;
    if (registry == NULL || registry->count > RUNA_MAX_MODULES) return NULL;
    for (index = 0u; index < registry->count; ++index) {
        const runa_module_t *module = registry->modules[index];
        if (module != NULL && module->module_id == module_id) return module;
    }
    return NULL;
}

runa_status_t runa_registry_validate(const runa_module_registry_t *registry) {
    uint8_t index;
    if (registry == NULL || registry->count > RUNA_MAX_MODULES) return RUNA_ERR_INVALID_FORMAT;
    for (index = 0u; index < registry->count; ++index) {
        const runa_module_t *module = registry->modules[index];
        uint8_t previous;
        if (module == NULL || module->module_id == 0u ||
            module->abi_version != RUNA_MODULE_ABI_VERSION ||
            module->validate == NULL || module->execute == NULL || module->validate_resource == NULL ||
            (module->legacy_operation_count != 0u && module->legacy_operations == NULL))
            return RUNA_ERR_INVALID_FORMAT;
        {
            uint8_t current_legacy;
            uint8_t previous_legacy;
            for (current_legacy = 0u; current_legacy < module->legacy_operation_count; ++current_legacy) {
                uint8_t opcode = module->legacy_operations[current_legacy].opcode;
                if (opcode == RUNA_OP_EXT || opcode == RUNA_OP_NOP || opcode == RUNA_OP_LOAD_CONST ||
                    opcode == RUNA_OP_MOV || opcode == RUNA_OP_ADD || opcode == RUNA_OP_SUB ||
                    opcode == RUNA_OP_AND || opcode == RUNA_OP_OR || opcode == RUNA_OP_XOR ||
                    opcode == RUNA_OP_NOT || opcode == RUNA_OP_SHL || opcode == RUNA_OP_SHR ||
                    (opcode >= RUNA_OP_CMP_EQ && opcode <= RUNA_OP_CMP_GE) ||
                    (opcode >= RUNA_OP_JUMP && opcode <= RUNA_OP_JUMP_IF_NOT) ||
                    opcode == RUNA_OP_DELAY_MS || opcode == RUNA_OP_EMIT || opcode == RUNA_OP_RETURN)
                    return RUNA_ERR_INVALID_FORMAT;
                for (previous_legacy = 0u; previous_legacy < current_legacy; ++previous_legacy) {
                    if (module->legacy_operations[previous_legacy].opcode == opcode)
                        return RUNA_ERR_INVALID_FORMAT;
                }
            }
        }
        for (previous = 0u; previous < index; ++previous) {
            if (registry->modules[previous]->module_id == module->module_id)
                return RUNA_ERR_INVALID_FORMAT;
            {
                uint8_t current_legacy;
                uint8_t previous_legacy;
                for (current_legacy = 0u; current_legacy < module->legacy_operation_count;
                     ++current_legacy) {
                    for (previous_legacy = 0u;
                         previous_legacy < registry->modules[previous]->legacy_operation_count;
                         ++previous_legacy) {
                        if (module->legacy_operations[current_legacy].opcode ==
                            registry->modules[previous]->legacy_operations[previous_legacy].opcode)
                            return RUNA_ERR_INVALID_FORMAT;
                    }
                }
            }
        }
    }
    return RUNA_OK;
}

runa_status_t runa_registry_find_legacy(const runa_module_registry_t *registry, uint8_t opcode,
                                        const runa_module_t **module, uint8_t *operation) {
    uint8_t index;
    if (module == NULL || operation == NULL || runa_registry_validate(registry) != RUNA_OK)
        return RUNA_ERR_INVALID_FORMAT;
    *module = NULL;
    *operation = 0u;
    for (index = 0u; index < registry->count; ++index) {
        const runa_module_t *candidate = registry->modules[index];
        uint8_t legacy_index;
        for (legacy_index = 0u; legacy_index < candidate->legacy_operation_count; ++legacy_index) {
            if (candidate->legacy_operations[legacy_index].opcode == opcode) {
                *module = candidate;
                *operation = candidate->legacy_operations[legacy_index].operation;
                return RUNA_OK;
            }
        }
    }
    return RUNA_ERR_INVALID_OPCODE;
}

runa_status_t runa_registry_validate_instruction(const runa_module_registry_t *registry,
                                               const runa_module_job_t *job,
                                               const runa_module_instruction_t *instruction,
                                               uint32_t *detail) {
    const runa_module_t *module;
    if (instruction == NULL) return RUNA_ERR_INVALID_FORMAT;
    module = runa_registry_find(registry, instruction->module_id);
    if (module == NULL) return RUNA_ERR_UNSUPPORTED_MODULE;
    return module->validate(module->context, job, instruction, detail);
}

static runa_status_t end_prefix(const runa_module_registry_t *registry,
                              const runa_module_job_t *job,
                              runa_status_t execution_status, uint8_t count) {
    runa_status_t first_error = RUNA_OK;
    while (count != 0u) {
        const runa_module_t *module = registry->modules[--count];
        if (module != NULL && module->end != NULL) {
            runa_status_t status = module->end(module->context, job, execution_status);
            if (first_error == RUNA_OK && status != RUNA_OK) first_error = status;
        }
    }
    return first_error;
}

runa_status_t runa_registry_begin_job(const runa_module_registry_t *registry,
                                    const runa_module_job_t *job) {
    uint8_t index;
    if (registry == NULL || registry->count > RUNA_MAX_MODULES) return RUNA_ERR_INVALID_FORMAT;
    for (index = 0u; index < registry->count; ++index) {
        const runa_module_t *module = registry->modules[index];
        runa_status_t status;
        if (module == NULL) return RUNA_ERR_INVALID_FORMAT;
        if (module->begin == NULL) continue;
        status = module->begin(module->context, job);
        if (status != RUNA_OK) {
            (void)end_prefix(registry, job, status, (uint8_t)(index + 1u));
            return status;
        }
    }
    return RUNA_OK;
}

runa_status_t runa_registry_end_job(const runa_module_registry_t *registry,
                                  const runa_module_job_t *job,
                                  runa_status_t execution_status) {
    if (registry == NULL || registry->count > RUNA_MAX_MODULES) return RUNA_ERR_INVALID_FORMAT;
    return end_prefix(registry, job, execution_status, registry->count);
}
