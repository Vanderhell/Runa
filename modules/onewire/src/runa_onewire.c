#include "runa_onewire.h"

#include "runa_ir.h"
#include "runa_resource.h"
#include "runa_result.h"

#include <string.h>

static const runa_onewire_hal_t *module_hal(void *context) {
    return (const runa_onewire_hal_t *)context;
}

uint8_t runa_onewire_rom_crc8(const uint8_t *rom_id) {
    uint8_t crc = 0u;
    uint8_t byte_index;
    if (rom_id == NULL) return 0u;
    for (byte_index = 0u; byte_index < 7u; ++byte_index) {
        uint8_t value = rom_id[byte_index];
        uint8_t bit;
        for (bit = 0u; bit < 8u; ++bit) {
            uint8_t mix = (uint8_t)((crc ^ value) & 1u);
            crc >>= 1u;
            if (mix != 0u) crc ^= 0x8cu;
            value >>= 1u;
        }
    }
    return crc;
}

int runa_onewire_rom_crc_valid(const uint8_t *rom_id) {
    return rom_id != NULL && runa_onewire_rom_crc8(rom_id) == rom_id[7];
}

static runa_status_t validate_resource(void *context, const runa_resource_t *resource,
                                       uint32_t *detail) {
    const runa_onewire_hal_t *hal = module_hal(context);
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (detail != NULL) *detail = resource->id;
    if (resource->resource_type == RUNA_ONEWIRE_BUS_RESOURCE_TYPE) {
        const runa_onewire_bus_resource_config_t *configuration =
            (const runa_onewire_bus_resource_config_t *)resource->config;
        if (configuration == NULL || configuration->maximum_tx_bytes == 0u ||
            configuration->maximum_tx_bytes > RUNA_ONEWIRE_MAX_TX_BYTES ||
            configuration->maximum_rx_bytes == 0u ||
            configuration->maximum_rx_bytes > RUNA_ONEWIRE_MAX_RX_BYTES ||
            configuration->maximum_search_results == 0u ||
            configuration->maximum_search_results > RUNA_ONEWIRE_MAX_SEARCH_RESULTS ||
            configuration->strong_pullup_supported > 1u ||
            configuration->broadcast_supported > 1u ||
            configuration->validate_rom_crc > 1u || configuration->reserved != 0u ||
            configuration->maximum_timeout_us == 0u ||
            configuration->maximum_timeout_us > RUNA_ONEWIRE_MAX_TIMEOUT_US ||
            (configuration->strong_pullup_supported != 0u && hal != NULL &&
             hal->strong_pullup_supported == 0u))
            return RUNA_ERR_INVALID_RESOURCE;
        return RUNA_OK;
    }
    if (resource->resource_type == RUNA_ONEWIRE_DEVICE_RESOURCE_TYPE) {
        const runa_onewire_device_resource_config_t *configuration =
            (const runa_onewire_device_resource_config_t *)resource->config;
        if (configuration == NULL || configuration->bus_resource_id == 0u ||
            configuration->reserved != 0u || !runa_onewire_rom_crc_valid(configuration->rom_id))
            return RUNA_ERR_INVALID_RESOURCE;
        return RUNA_OK;
    }
    return RUNA_ERR_RESOURCE_TYPE;
}

static runa_status_t resource_bus(const runa_module_job_t *job, const runa_resource_t *resource,
                                  const runa_resource_t **bus_resource,
                                  const runa_onewire_bus_resource_config_t **configuration) {
    const runa_resource_t *bus = resource;
    const runa_onewire_device_resource_config_t *device_configuration;
    if (job == NULL || resource == NULL || bus_resource == NULL || configuration == NULL)
        return RUNA_ERR_INVALID_FORMAT;
    if (resource->resource_type == RUNA_ONEWIRE_DEVICE_RESOURCE_TYPE) {
        device_configuration =
            (const runa_onewire_device_resource_config_t *)resource->config;
        if (device_configuration == NULL) return RUNA_ERR_INVALID_RESOURCE;
        bus = runa_resource_find(job->resources, device_configuration->bus_resource_id);
        if (bus == NULL || bus->module_id != RUNA_ONEWIRE_MODULE_ID ||
            bus->resource_type != RUNA_ONEWIRE_BUS_RESOURCE_TYPE ||
            bus->platform_handle != resource->platform_handle)
            return RUNA_ERR_RESOURCE_TYPE;
    }
    if (bus->module_id != RUNA_ONEWIRE_MODULE_ID ||
        bus->resource_type != RUNA_ONEWIRE_BUS_RESOURCE_TYPE || bus->config == NULL)
        return RUNA_ERR_RESOURCE_TYPE;
    *bus_resource = bus;
    *configuration = (const runa_onewire_bus_resource_config_t *)bus->config;
    return RUNA_OK;
}

static runa_status_t validate_timeout(uint32_t timeout_us,
                                      const runa_onewire_bus_resource_config_t *configuration) {
    if (timeout_us == 0u || timeout_us > RUNA_ONEWIRE_MAX_TIMEOUT_US ||
        configuration == NULL || timeout_us > configuration->maximum_timeout_us)
        return RUNA_ERR_OUT_OF_RANGE;
    return RUNA_OK;
}

static runa_status_t validate(void *context, const runa_module_job_t *job,
                              const runa_module_instruction_t *instruction, uint32_t *detail) {
    const runa_onewire_hal_t *hal = module_hal(context);
    const runa_resource_t *resource;
    const runa_resource_t *bus_resource;
    const runa_onewire_bus_resource_config_t *configuration;
    const uint8_t *operands;
    uint16_t resource_id;
    uint8_t permission;
    runa_status_t status;
    if (instruction == NULL || job == NULL || instruction->operands == NULL)
        return RUNA_ERR_INVALID_FORMAT;
    operands = instruction->operands;
    if (detail != NULL && instruction->operand_size >= 2u)
        *detail = runa_read_u16_le(operands);
    if (instruction->operation != RUNA_ONEWIRE_OP_RESET &&
        instruction->operation != RUNA_ONEWIRE_OP_TRANSFER &&
        instruction->operation != RUNA_ONEWIRE_OP_ROM_SEARCH)
        return RUNA_ERR_INVALID_OPCODE;
    if (instruction->operand_size < 2u) return RUNA_ERR_INVALID_OPERAND;
    resource_id = runa_read_u16_le(operands);
    resource = runa_resource_find(job->resources, resource_id);
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (resource->module_id != RUNA_ONEWIRE_MODULE_ID) return RUNA_ERR_RESOURCE_TYPE;
    status = validate_resource(context, resource, detail);
    if (status != RUNA_OK) return status;
    status = resource_bus(job, resource, &bus_resource, &configuration);
    if (status != RUNA_OK) return status;
    (void)bus_resource;

    if (instruction->operation == RUNA_ONEWIRE_OP_RESET) {
        uint32_t timeout_us;
        if (instruction->operand_size != 7u) return RUNA_ERR_INVALID_OPERAND;
        if (operands[6] >= RUNA_REGISTER_COUNT) return RUNA_ERR_INVALID_REGISTER;
        timeout_us = runa_read_u32_le(operands + 2u);
        status = validate_timeout(timeout_us, configuration);
        if (status != RUNA_OK) return status;
        permission = (uint8_t)RUNA_PERMISSION_READ;
    } else if (instruction->operation == RUNA_ONEWIRE_OP_ROM_SEARCH) {
        uint8_t maximum_results;
        uint32_t timeout_us;
        if (resource->resource_type != RUNA_ONEWIRE_BUS_RESOURCE_TYPE ||
            instruction->operand_size != 7u)
            return RUNA_ERR_RESOURCE_TYPE;
        maximum_results = operands[2];
        timeout_us = runa_read_u32_le(operands + 3u);
        if (maximum_results == 0u || maximum_results > RUNA_ONEWIRE_MAX_SEARCH_RESULTS ||
            maximum_results > configuration->maximum_search_results)
            return RUNA_ERR_OUT_OF_RANGE;
        status = validate_timeout(timeout_us, configuration);
        if (status != RUNA_OK) return status;
        permission = (uint8_t)RUNA_PERMISSION_READ;
    } else {
        uint8_t flags;
        uint8_t transmit_size;
        uint8_t receive_size;
        size_t base_size = 9u;
        uint32_t timeout_us;
        if (instruction->operand_size < base_size) return RUNA_ERR_INVALID_OPERAND;
        flags = operands[2];
        transmit_size = operands[3];
        receive_size = operands[4];
        timeout_us = runa_read_u32_le(operands + 5u);
        if ((flags & 0xfcu) != 0u ||
            (transmit_size == 0u && receive_size == 0u))
            return RUNA_ERR_INVALID_OPERAND;
        if ((flags & RUNA_ONEWIRE_TRANSFER_STRONG_PULLUP) != 0u) base_size += 4u;
        if ((size_t)instruction->operand_size != base_size + transmit_size)
            return RUNA_ERR_INVALID_OPERAND;
        if (transmit_size > RUNA_ONEWIRE_MAX_TX_BYTES ||
            receive_size > RUNA_ONEWIRE_MAX_RX_BYTES ||
            transmit_size > configuration->maximum_tx_bytes ||
            receive_size > configuration->maximum_rx_bytes)
            return RUNA_ERR_OUT_OF_RANGE;
        if (resource->resource_type == RUNA_ONEWIRE_BUS_RESOURCE_TYPE) {
            if ((flags & RUNA_ONEWIRE_TRANSFER_SKIP_ROM) == 0u ||
                configuration->broadcast_supported == 0u)
                return RUNA_ERR_ACCESS_DENIED;
        } else if ((flags & RUNA_ONEWIRE_TRANSFER_SKIP_ROM) != 0u) {
            return RUNA_ERR_ACCESS_DENIED;
        }
        if ((flags & RUNA_ONEWIRE_TRANSFER_STRONG_PULLUP) != 0u) {
            uint32_t duration_us = runa_read_u32_le(operands + 9u);
            if (configuration->strong_pullup_supported == 0u || hal == NULL ||
                hal->strong_pullup_supported == 0u || duration_us == 0u ||
                duration_us > RUNA_ONEWIRE_MAX_STRONG_PULLUP_US)
                return RUNA_ERR_OUT_OF_RANGE;
            permission = (uint8_t)(RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE);
        } else {
            permission = 0u;
            if (transmit_size != 0u) permission |= (uint8_t)RUNA_PERMISSION_WRITE;
            if (receive_size != 0u) permission |= (uint8_t)RUNA_PERMISSION_READ;
        }
        status = validate_timeout(timeout_us, configuration);
        if (status != RUNA_OK) return status;
    }
    if ((resource->permissions & permission) != permission) return RUNA_ERR_ACCESS_DENIED;
    return RUNA_OK;
}

static runa_status_t execute(void *context, runa_module_job_t *job,
                             const runa_module_instruction_t *instruction, uint32_t *detail) {
    runa_onewire_hal_t *hal = (runa_onewire_hal_t *)context;
    const uint8_t *operands = instruction->operands;
    const runa_resource_t *resource = runa_resource_find(job->resources,
                                                         runa_read_u16_le(operands));
    const runa_onewire_device_resource_config_t *device_configuration;
    uint8_t receive[RUNA_ONEWIRE_MAX_RX_BYTES] = {0u};
    runa_status_t status;
    if (detail != NULL) *detail = runa_read_u16_le(operands);
    if (resource == NULL || hal == NULL) return RUNA_ERR_INTERNAL;
    if (instruction->operation == RUNA_ONEWIRE_OP_RESET) {
        uint8_t presence = 0u;
        status = hal->reset == NULL ? RUNA_ERR_INTERNAL :
            hal->reset(hal->context, resource->platform_handle,
                       runa_read_u32_le(operands + 2u), &presence);
        if (status == RUNA_OK) job->registers[operands[6]] = presence == 0u ? 0u : 1u;
        return status;
    }
    if (instruction->operation == RUNA_ONEWIRE_OP_ROM_SEARCH) {
        uint8_t rom_ids[RUNA_ONEWIRE_MAX_SEARCH_RESULTS * 8u] = {0u};
        size_t result_count = 0u;
        size_t index;
        if (hal->search == NULL) return RUNA_ERR_INTERNAL;
        status = hal->search(hal->context, resource->platform_handle, rom_ids, sizeof rom_ids,
                             operands[2], &result_count, runa_read_u32_le(operands + 3u));
        if (status != RUNA_OK) return status;
        if (result_count > (size_t)operands[2] || result_count > RUNA_ONEWIRE_MAX_SEARCH_RESULTS)
            return RUNA_ONEWIRE_ERR_SEARCH;
        {
            const runa_onewire_bus_resource_config_t *configuration =
                (const runa_onewire_bus_resource_config_t *)resource->config;
            if (configuration->validate_rom_crc != 0u) {
                for (index = 0u; index < result_count; ++index) {
                    if (!runa_onewire_rom_crc_valid(rom_ids + index * 8u))
                        return RUNA_ONEWIRE_ERR_CRC;
                }
            }
        }
        for (index = 0u; index < result_count; ++index) {
            if (job->emit_data == NULL) return RUNA_ERR_INTERNAL;
            status = job->emit_data(job->emit_context, RUNA_ONEWIRE_MODULE_ID,
                                    instruction->instruction_index, (uint16_t)index,
                                    rom_ids + index * 8u, 8u);
            if (status != RUNA_OK) return status;
        }
        return RUNA_OK;
    }
    {
        uint8_t flags = operands[2];
        uint8_t transmit_size = operands[3];
        uint8_t receive_size = operands[4];
        uint32_t strong_pullup_us = 0u;
        const uint8_t *rom_id = NULL;
        const uint8_t *transmit = operands + 9u;
        if ((flags & RUNA_ONEWIRE_TRANSFER_STRONG_PULLUP) != 0u) {
            strong_pullup_us = runa_read_u32_le(operands + 9u);
            transmit += 4u;
        }
        if (resource->resource_type == RUNA_ONEWIRE_DEVICE_RESOURCE_TYPE) {
            device_configuration =
                (const runa_onewire_device_resource_config_t *)resource->config;
            rom_id = device_configuration->rom_id;
        }
        if (hal->transfer == NULL) return RUNA_ERR_INTERNAL;
        status = hal->transfer(hal->context, resource->platform_handle, rom_id,
                               transmit_size == 0u ? NULL : transmit, transmit_size,
                               receive_size == 0u ? NULL : receive, receive_size,
                               runa_read_u32_le(operands + 5u), strong_pullup_us);
        if (status != RUNA_OK) return status;
        if (receive_size != 0u) {
            size_t offset = 0u;
            uint16_t sequence = 0u;
            while (offset < receive_size) {
                size_t chunk = (size_t)receive_size - offset;
                if (chunk > RUNA_MAX_MODULE_DATA_BYTES) chunk = RUNA_MAX_MODULE_DATA_BYTES;
                if (job->emit_data == NULL) return RUNA_ERR_INTERNAL;
                status = job->emit_data(job->emit_context, RUNA_ONEWIRE_MODULE_ID,
                                        instruction->instruction_index, sequence,
                                        receive + offset, chunk);
                if (status != RUNA_OK) return status;
                offset += chunk;
                ++sequence;
            }
        }
    }
    return RUNA_OK;
}

static size_t capabilities(void *context, uint8_t *output, size_t capacity) {
    const runa_onewire_hal_t *hal = module_hal(context);
    uint8_t flags = (uint8_t)(RUNA_ONEWIRE_CAP_BROADCAST |
                              RUNA_ONEWIRE_CAP_DEVICE_RESOURCE |
                              RUNA_ONEWIRE_CAP_ROM_CRC);
    if (hal != NULL && hal->strong_pullup_supported != 0u)
        flags |= RUNA_ONEWIRE_CAP_STRONG_PULLUP;
    if (output != NULL && capacity >= RUNA_ONEWIRE_CAPABILITY_SIZE) {
        output[0] = RUNA_ONEWIRE_CAPABILITY_VERSION;
        output[1] = (uint8_t)((1u << (RUNA_ONEWIRE_OP_RESET - 1u)) |
                              (1u << (RUNA_ONEWIRE_OP_TRANSFER - 1u)) |
                              (1u << (RUNA_ONEWIRE_OP_ROM_SEARCH - 1u)));
        runa_write_u16_le(output + 2u, RUNA_ONEWIRE_MAX_TX_BYTES);
        runa_write_u16_le(output + 4u, RUNA_ONEWIRE_MAX_RX_BYTES);
        output[6] = RUNA_ONEWIRE_MAX_SEARCH_RESULTS;
        output[7] = flags;
        runa_write_u32_le(output + 8u, RUNA_ONEWIRE_MAX_TIMEOUT_US);
        output[12] = RUNA_ONEWIRE_BUS_RESOURCE_TYPE;
        output[13] = RUNA_ONEWIRE_DEVICE_RESOURCE_TYPE;
        output[14] = 8u;
        output[15] = 0u;
    }
    return RUNA_ONEWIRE_CAPABILITY_SIZE;
}

runa_module_t runa_onewire_module(runa_onewire_hal_t *hal) {
    runa_module_t module = { RUNA_ONEWIRE_MODULE_ID, RUNA_MODULE_ABI_VERSION, 1u,
                             validate, execute, NULL, NULL, capabilities, hal,
                             NULL, 0u, validate_resource };
    return module;
}
