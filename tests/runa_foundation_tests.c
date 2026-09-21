#include "runa_registry.h"
#include "runa_resource.h"
#include "runa_ir.h"
#include "runa_capabilities.h"
#include "runa_validator.h"

#include <string.h>
#include <stdio.h>

static runa_status_t validate(void *context, const runa_module_job_t *job,
                             const runa_module_instruction_t *instruction, uint32_t *detail) {
    (void)context;
    (void)job;
    (void)detail;
    return instruction != NULL && instruction->operation == 1u ? RUNA_OK : RUNA_ERR_INVALID_OPCODE;
}

static runa_status_t execute(void *context, runa_module_job_t *job,
                            const runa_module_instruction_t *instruction, uint32_t *detail) {
    (void)context;
    (void)job;
    (void)instruction;
    (void)detail;
    return RUNA_OK;
}

static runa_status_t validate_resource(void *context, const runa_resource_t *resource,
                                       uint32_t *detail) {
    (void)context;
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (detail != NULL) *detail = resource->id;
    return resource->resource_type == 1u ? RUNA_OK : RUNA_ERR_RESOURCE_TYPE;
}

int main(void) {
    runa_module_registry_t registry;
    const runa_module_t module = { 7u, RUNA_MODULE_ABI_VERSION, 1u, validate, execute,
                                   NULL, NULL, NULL, NULL, NULL, 0u, validate_resource };
    runa_module_t duplicate = module;
    static runa_module_t additional[RUNA_MAX_MODULES - 1u];
    runa_resource_t item = { 4u, 7u, 1u, 0u, RUNA_PERMISSION_READ, 0u, NULL };
    runa_resource_table_t resources = { &item, 1u };
    uint8_t index;
    runa_registry_init(&registry);
    if (runa_registry_add(&registry, NULL) != RUNA_ERR_INVALID_FORMAT) return 1;
    if (runa_registry_add(&registry, &module) != RUNA_OK) return 2;
    if (runa_registry_add(&registry, &duplicate) != RUNA_ERR_INVALID_FORMAT) return 3;
    if (runa_registry_find(&registry, 7u) != &module) return 4;
    if (runa_registry_find(&registry, 8u) != NULL) return 5;
    if (runa_resource_table_validate(&resources, &registry) != RUNA_OK) return 6;
    item.module_id = 9u;
    if (runa_resource_table_validate(&resources, &registry) != RUNA_ERR_INVALID_RESOURCE) return 7;
    item.module_id = 7u;
    item.resource_type = 2u;
    if (runa_resource_table_validate(&resources, &registry) != RUNA_ERR_RESOURCE_TYPE) return 11;
    item.resource_type = 1u;
    {
        uint8_t job[48] = {0};
        runa_resource_table_t empty_resources = { NULL, 0u };
        runa_decoded_job_t decoded;
        runa_validation_error_t error;
        job[0] = (uint8_t)'J'; job[1] = (uint8_t)'E';
        job[2] = (uint8_t)'X'; job[3] = (uint8_t)'E';
        job[4] = RUNA_PROTOCOL_VERSION; job[5] = RUNA_IR_VERSION_V2;
        runa_write_u16_le(job + 6u, RUNA_HEADER_SIZE);
        runa_write_u32_le(job + 12u, sizeof job);
        runa_write_u32_le(job + 16u, 8u);
        runa_write_u16_le(job + 20u, 2u);
        runa_write_u32_le(job + 24u, 10u);
        runa_write_u32_le(job + 28u, 1000u);
        runa_write_u16_le(job + 32u, RUNA_MAX_RESULT_BYTES);
        runa_write_u16_le(job + 34u, 1u);
        runa_write_u32_le(job + 36u, 4u);
        job[40] = RUNA_OP_EXT; job[41] = 3u;
        runa_write_u16_le(job + 42u, 7u); job[44] = 1u;
        job[45] = RUNA_OP_RETURN; job[46] = 1u; job[47] = 0u;
        if (runa_validate(job, sizeof job, &registry, &empty_resources, &decoded, &error) != RUNA_OK)
            return 12;
        runa_write_u16_le(job + 42u, 8u);
        if (runa_validate(job, sizeof job, &registry, &empty_resources, &decoded, &error) !=
            RUNA_ERR_UNSUPPORTED_MODULE) return 13;
        runa_write_u16_le(job + 42u, 7u); job[44] = 2u;
        if (runa_validate(job, sizeof job, &registry, &empty_resources, &decoded, &error) !=
            RUNA_ERR_INVALID_OPCODE) return 14;
        job[5] = 3u;
        if (runa_validate(job, sizeof job, &registry, &empty_resources, &decoded, &error) !=
            RUNA_ERR_UNSUPPORTED_IR_VERSION) return 15;
        job[5] = RUNA_IR_VERSION_V2; job[41] = 2u;
        runa_write_u32_le(job + 12u, 47u); runa_write_u32_le(job + 16u, 7u);
        if (runa_validate(job, 47u, &registry, &empty_resources, &decoded, &error) !=
            RUNA_ERR_INVALID_OPERAND) return 16;
    }
    item.id = 5u;
    {
        runa_resource_t pair[2] = { item, item };
        runa_resource_table_t duplicates = { pair, 2u };
        if (runa_resource_table_validate(&duplicates, &registry) != RUNA_ERR_INVALID_RESOURCE) return 8;
    }
    for (index = 1u; index < RUNA_MAX_MODULES; ++index) {
        additional[index - 1u] = module;
        additional[index - 1u].module_id = (uint16_t)(7u + index);
        if (runa_registry_add(&registry, &additional[index - 1u]) != RUNA_OK) return 9;
    }
    duplicate.module_id = 100u;
    if (runa_registry_add(&registry, &duplicate) != RUNA_ERR_OUT_OF_RANGE) return 10;
    {
        uint8_t capabilities[RUNA_MAX_CAPABILITY_BYTES];
        size_t written = 0u;
        runa_capabilities_view_t view;
        if (runa_capabilities_encode(&registry, capabilities, sizeof capabilities, &written) != RUNA_OK ||
            runa_capabilities_decode(capabilities, written, &view) != RUNA_OK ||
            view.module_count != RUNA_MAX_MODULES) return 17;
        {
            uint8_t malformed[RUNA_MAX_CAPABILITY_BYTES];
            memcpy(malformed, capabilities, written);
            runa_write_u16_le(malformed + 36u, 0u);
            if (runa_capabilities_decode(malformed, written, &view) != RUNA_ERR_INVALID_FORMAT)
                return 18;
            memcpy(malformed, capabilities, written);
            malformed[38u] = 0u;
            if (runa_capabilities_decode(malformed, written, &view) != RUNA_ERR_INVALID_FORMAT)
                return 19;
        }
    }
    puts("runa foundation registry/resource checks passed");
    return 0;
}
