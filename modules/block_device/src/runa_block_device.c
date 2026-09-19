#include "runa_block_device.h"

#include "runa_ir.h"
#include "runa_resource.h"
#include "runa_result.h"

static const runa_block_device_resource_config_t *resource_config(
    const runa_resource_t *resource) {
    return resource == NULL ? NULL :
        (const runa_block_device_resource_config_t *)resource->config;
}

static runa_status_t validate_resource(void *context, const runa_resource_t *resource,
                                       uint32_t *detail) {
    const runa_block_device_resource_config_t *configuration;
    (void)context;
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (detail != NULL) *detail = resource->id;
    if (resource->module_id != RUNA_BLOCK_DEVICE_MODULE_ID ||
        resource->resource_type != RUNA_BLOCK_DEVICE_RESOURCE_TYPE)
        return RUNA_ERR_RESOURCE_TYPE;
    configuration = resource_config(resource);
    if (configuration == NULL || configuration->capacity_bytes == 0u ||
        configuration->read_alignment == 0u || configuration->write_alignment == 0u ||
        configuration->erase_alignment == 0u ||
        (configuration->capability_flags & (uint8_t)~(RUNA_BLOCK_CAP_READ |
            RUNA_BLOCK_CAP_WRITE | RUNA_BLOCK_CAP_ERASE | RUNA_BLOCK_CAP_SYNC |
            RUNA_BLOCK_CAP_ERASE_REQUIRED_BEFORE_WRITE | RUNA_BLOCK_CAP_PERSISTENT)) != 0u ||
        configuration->max_read_bytes > RUNA_BLOCK_MAX_READ_BYTES ||
        configuration->max_write_bytes > RUNA_BLOCK_MAX_WRITE_BYTES ||
        configuration->max_erase_bytes > RUNA_BLOCK_MAX_ERASE_BYTES ||
        ((configuration->capability_flags & RUNA_BLOCK_CAP_READ) != 0u &&
         configuration->max_read_bytes == 0u) ||
        ((configuration->capability_flags & RUNA_BLOCK_CAP_WRITE) != 0u &&
         configuration->max_write_bytes == 0u) ||
        ((configuration->capability_flags & RUNA_BLOCK_CAP_ERASE) != 0u &&
         configuration->max_erase_bytes == 0u) ||
        (configuration->atomic_write_size != 0u &&
         ((configuration->capability_flags & RUNA_BLOCK_CAP_WRITE) == 0u ||
          configuration->atomic_write_size > configuration->max_write_bytes)) ||
        configuration->reserved != 0u)
        return RUNA_ERR_INVALID_RESOURCE;
    return RUNA_OK;
}

static runa_status_t check_range(const runa_block_device_resource_config_t *configuration,
                                 uint8_t operation, uint32_t offset, uint32_t length) {
    uint32_t alignment;
    uint32_t maximum;
    if (configuration == NULL || length == 0u || offset > configuration->capacity_bytes ||
        length > configuration->capacity_bytes - offset)
        return RUNA_ERR_OUT_OF_RANGE;
    switch (operation) {
    case RUNA_BLOCK_DEVICE_OP_READ:
        alignment = configuration->read_alignment;
        maximum = configuration->max_read_bytes;
        break;
    case RUNA_BLOCK_DEVICE_OP_WRITE:
        alignment = configuration->write_alignment;
        maximum = configuration->max_write_bytes;
        break;
    case RUNA_BLOCK_DEVICE_OP_ERASE:
        alignment = configuration->erase_alignment;
        maximum = configuration->max_erase_bytes;
        break;
    default:
        return RUNA_ERR_INVALID_OPCODE;
    }
    if (maximum == 0u) return RUNA_ERR_UNSUPPORTED_MODULE;
    if (length > maximum || offset % alignment != 0u || length % alignment != 0u)
        return RUNA_ERR_OUT_OF_RANGE;
    return RUNA_OK;
}

static runa_status_t validate(void *context, const runa_module_job_t *job,
                              const runa_module_instruction_t *instruction, uint32_t *detail) {
    const runa_block_device_resource_config_t *configuration;
    const runa_resource_t *resource;
    const uint8_t *operands;
    uint16_t resource_id;
    uint32_t offset = 0u;
    uint32_t length = 0u;
    uint8_t permission;
    uint8_t capability;
    runa_status_t status;
    (void)context;
    if (instruction == NULL || job == NULL || instruction->operands == NULL)
        return RUNA_ERR_INVALID_FORMAT;
    if (instruction->operation < RUNA_BLOCK_DEVICE_OP_READ ||
        instruction->operation > RUNA_BLOCK_DEVICE_OP_SYNC)
        return RUNA_ERR_INVALID_OPCODE;
    operands = instruction->operands;
    if (instruction->operation == RUNA_BLOCK_DEVICE_OP_SYNC) {
        if (instruction->operand_size != 2u) return RUNA_ERR_INVALID_OPERAND;
    } else {
        if (instruction->operand_size < 10u) return RUNA_ERR_INVALID_OPERAND;
        offset = runa_read_u32_le(operands + 2u);
        length = runa_read_u32_le(operands + 6u);
        if (instruction->operation == RUNA_BLOCK_DEVICE_OP_WRITE &&
            (size_t)instruction->operand_size != 10u + (size_t)length)
            return RUNA_ERR_INVALID_OPERAND;
        if (instruction->operation != RUNA_BLOCK_DEVICE_OP_WRITE &&
            instruction->operand_size != 10u)
            return RUNA_ERR_INVALID_OPERAND;
    }
    resource_id = runa_read_u16_le(operands);
    if (detail != NULL) *detail = resource_id;
    resource = runa_resource_find(job->resources, resource_id);
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (resource->module_id != RUNA_BLOCK_DEVICE_MODULE_ID ||
        resource->resource_type != RUNA_BLOCK_DEVICE_RESOURCE_TYPE)
        return RUNA_ERR_RESOURCE_TYPE;
    status = validate_resource(context, resource, detail);
    if (status != RUNA_OK) return status;
    configuration = resource_config(resource);
    if (instruction->operation == RUNA_BLOCK_DEVICE_OP_READ) {
        permission = (uint8_t)RUNA_PERMISSION_READ;
        capability = RUNA_BLOCK_CAP_READ;
    } else if (instruction->operation == RUNA_BLOCK_DEVICE_OP_WRITE) {
        permission = (uint8_t)RUNA_PERMISSION_WRITE;
        capability = RUNA_BLOCK_CAP_WRITE;
    } else if (instruction->operation == RUNA_BLOCK_DEVICE_OP_ERASE) {
        permission = (uint8_t)RUNA_PERMISSION_WRITE;
        capability = RUNA_BLOCK_CAP_ERASE;
    } else {
        permission = (uint8_t)RUNA_PERMISSION_WRITE;
        capability = RUNA_BLOCK_CAP_SYNC;
    }
    if ((configuration->capability_flags & capability) == 0u)
        return RUNA_ERR_UNSUPPORTED_MODULE;
    if ((resource->permissions & permission) != permission) return RUNA_ERR_ACCESS_DENIED;
    if (instruction->operation != RUNA_BLOCK_DEVICE_OP_SYNC)
        return check_range(configuration, instruction->operation, offset, length);
    return RUNA_OK;
}

static runa_status_t execute(void *context, runa_module_job_t *job,
                             const runa_module_instruction_t *instruction, uint32_t *detail) {
    runa_block_device_hal_t *hal = (runa_block_device_hal_t *)context;
    const uint8_t *operands = instruction->operands;
    uint16_t resource_id = runa_read_u16_le(operands);
    const runa_resource_t *resource = runa_resource_find(job->resources, resource_id);
    uint32_t offset;
    uint32_t length;
    runa_status_t status;
    if (detail != NULL) *detail = resource_id;
    if (resource == NULL || hal == NULL) return RUNA_ERR_INTERNAL;
    if (instruction->operation == RUNA_BLOCK_DEVICE_OP_SYNC) {
        return hal->sync == NULL ? RUNA_ERR_INTERNAL :
            hal->sync(hal->context, resource->platform_handle);
    }
    offset = runa_read_u32_le(operands + 2u);
    length = runa_read_u32_le(operands + 6u);
    if (instruction->operation == RUNA_BLOCK_DEVICE_OP_READ) {
        uint8_t buffer[RUNA_MAX_MODULE_DATA_BYTES];
        uint32_t current_offset = offset;
        size_t remaining = (size_t)length;
        uint16_t sequence = 0u;
        if (hal->read == NULL || job->emit_data == NULL) return RUNA_ERR_INTERNAL;
        while (remaining != 0u) {
            size_t chunk = remaining > sizeof buffer ? sizeof buffer : remaining;
            status = hal->read(hal->context, resource->platform_handle, current_offset,
                               buffer, chunk);
            if (status != RUNA_OK) return status;
            status = job->emit_data(job->emit_context, RUNA_BLOCK_DEVICE_MODULE_ID,
                                    instruction->instruction_index, sequence, buffer, chunk);
            if (status != RUNA_OK) return status;
            current_offset += (uint32_t)chunk;
            remaining -= chunk;
            ++sequence;
        }
        return RUNA_OK;
    }
    if (instruction->operation == RUNA_BLOCK_DEVICE_OP_WRITE) {
        return hal->write == NULL ? RUNA_ERR_INTERNAL :
            hal->write(hal->context, resource->platform_handle, offset, operands + 10u,
                       (size_t)length);
    }
    return hal->erase == NULL ? RUNA_ERR_INTERNAL :
        hal->erase(hal->context, resource->platform_handle, offset, (size_t)length);
}

static size_t capabilities(void *context, uint8_t *output, size_t capacity) {
    (void)context;
    if (output != NULL && capacity >= RUNA_BLOCK_CAPABILITY_SIZE) {
        output[0] = 1u;
        output[1] = (uint8_t)(RUNA_BLOCK_CAP_READ | RUNA_BLOCK_CAP_WRITE |
                              RUNA_BLOCK_CAP_ERASE | RUNA_BLOCK_CAP_SYNC);
        output[2] = RUNA_BLOCK_OFFSET_WIDTH_BYTES;
        output[3] = RUNA_BLOCK_DEVICE_RESOURCE_TYPE;
        runa_write_u16_le(output + 4u, (uint16_t)RUNA_BLOCK_MAX_READ_BYTES);
        runa_write_u16_le(output + 6u, (uint16_t)RUNA_BLOCK_MAX_WRITE_BYTES);
        runa_write_u16_le(output + 8u, (uint16_t)RUNA_BLOCK_MAX_ERASE_BYTES);
        runa_write_u16_le(output + 10u, (uint16_t)RUNA_MAX_MODULE_DATA_BYTES);
    }
    return RUNA_BLOCK_CAPABILITY_SIZE;
}

runa_module_t runa_block_device_module(runa_block_device_hal_t *hal) {
    runa_module_t module = { RUNA_BLOCK_DEVICE_MODULE_ID, RUNA_MODULE_ABI_VERSION, 1u,
                             validate, execute, NULL, NULL, capabilities, hal,
                             NULL, 0u, validate_resource };
    return module;
}
