#include "runa_pulse.h"

#include "runa_ir.h"
#include "runa_resource.h"

static uint8_t edge_bit(uint8_t edge) {
    return (uint8_t)(edge <= RUNA_PULSE_EDGE_BOTH ? (1u << edge) : 0u);
}

static runa_status_t validate_resource(void *context, const runa_resource_t *resource,
                                       uint32_t *detail) {
    const runa_pulse_resource_config_t *configuration;
    (void)context;
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (detail != NULL) *detail = resource->id;
    if (resource->module_id != RUNA_PULSE_MODULE_ID ||
        resource->resource_type != RUNA_PULSE_RESOURCE_TYPE)
        return RUNA_ERR_RESOURCE_TYPE;
    configuration = (const runa_pulse_resource_config_t *)resource->config;
    if (configuration == NULL || configuration->minimum_window_us == 0u ||
        configuration->maximum_window_us < configuration->minimum_window_us ||
        configuration->maximum_window_us > RUNA_PULSE_MAX_WINDOW_US ||
        configuration->minimum_timeout_us == 0u ||
        configuration->maximum_timeout_us < configuration->minimum_timeout_us ||
        configuration->maximum_timeout_us > RUNA_PULSE_MAX_TIMEOUT_US ||
        configuration->maximum_count == 0u ||
        configuration->maximum_count > RUNA_PULSE_MAX_COUNT ||
        configuration->supported_edges > 7u ||
        configuration->supported_edges == 0u ||
        configuration->supported_levels > (RUNA_PULSE_LEVEL_HIGH | RUNA_PULSE_LEVEL_LOW) ||
        configuration->supported_levels == 0u || configuration->reserved != 0u)
        return RUNA_ERR_INVALID_RESOURCE;
    return RUNA_OK;
}

static runa_status_t validate_common(const runa_module_job_t *job,
                                     const runa_module_instruction_t *instruction,
                                     const runa_resource_t **resource,
                                     const runa_pulse_resource_config_t **configuration,
                                     uint8_t *destination, uint32_t *detail) {
    uint16_t resource_id;
    if (job == NULL || instruction == NULL || instruction->operands == NULL)
        return RUNA_ERR_INVALID_FORMAT;
    resource_id = runa_read_u16_le(instruction->operands);
    if (detail != NULL) *detail = resource_id;
    *resource = runa_resource_find(job->resources, resource_id);
    if (*resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if ((*resource)->module_id != RUNA_PULSE_MODULE_ID ||
        (*resource)->resource_type != RUNA_PULSE_RESOURCE_TYPE)
        return RUNA_ERR_RESOURCE_TYPE;
    if (((*resource)->permissions & RUNA_PERMISSION_READ) == 0u)
        return RUNA_ERR_ACCESS_DENIED;
    if (validate_resource(NULL, *resource, detail) != RUNA_OK)
        return RUNA_ERR_INVALID_RESOURCE;
    *configuration = (const runa_pulse_resource_config_t *)(*resource)->config;
    *destination = instruction->operands[instruction->operand_size - 1u];
    if (*destination >= job->register_count) return RUNA_ERR_INVALID_REGISTER;
    return RUNA_OK;
}

static runa_status_t validate(void *context, const runa_module_job_t *job,
                              const runa_module_instruction_t *instruction, uint32_t *detail) {
    const runa_resource_t *resource;
    const runa_pulse_resource_config_t *configuration;
    uint8_t destination;
    uint8_t edge_or_level;
    uint32_t bound;
    uint32_t maximum_count;
    runa_status_t status;
    (void)context;
    if (instruction == NULL || instruction->operands == NULL) return RUNA_ERR_INVALID_OPERAND;
    if ((instruction->operation == RUNA_PULSE_OP_COUNT && instruction->operand_size != 12u) ||
        ((instruction->operation == RUNA_PULSE_OP_MEASURE_WIDTH ||
          instruction->operation == RUNA_PULSE_OP_MEASURE_PERIOD) &&
         instruction->operand_size != 8u))
        return RUNA_ERR_INVALID_OPERAND;
    if (instruction->operation != RUNA_PULSE_OP_COUNT &&
        instruction->operation != RUNA_PULSE_OP_MEASURE_WIDTH &&
        instruction->operation != RUNA_PULSE_OP_MEASURE_PERIOD)
        return RUNA_ERR_INVALID_OPCODE;
    status = validate_common(job, instruction, &resource, &configuration, &destination, detail);
    if (status != RUNA_OK) return status;
    (void)resource;
    if (instruction->operation == RUNA_PULSE_OP_COUNT) {
        edge_or_level = instruction->operands[2];
        bound = runa_read_u32_le(instruction->operands + 3u);
        maximum_count = runa_read_u32_le(instruction->operands + 7u);
        if (edge_bit(edge_or_level) == 0u ||
            (configuration->supported_edges & edge_bit(edge_or_level)) == 0u)
            return RUNA_ERR_INVALID_OPERAND;
        if (bound < configuration->minimum_window_us ||
            bound > configuration->maximum_window_us || bound > job->max_runtime_us)
            return RUNA_ERR_OUT_OF_RANGE;
        if (maximum_count == 0u || maximum_count > configuration->maximum_count)
            return RUNA_ERR_OUT_OF_RANGE;
    } else if (instruction->operation == RUNA_PULSE_OP_MEASURE_WIDTH) {
        edge_or_level = instruction->operands[2];
        bound = runa_read_u32_le(instruction->operands + 3u);
        if (edge_or_level != RUNA_PULSE_LEVEL_HIGH && edge_or_level != RUNA_PULSE_LEVEL_LOW)
            return RUNA_ERR_INVALID_OPERAND;
        if ((configuration->supported_levels & edge_or_level) == 0u)
            return RUNA_ERR_INVALID_OPERAND;
        if (bound < configuration->minimum_timeout_us ||
            bound > configuration->maximum_timeout_us || bound > job->max_runtime_us)
            return RUNA_ERR_OUT_OF_RANGE;
    } else {
        edge_or_level = instruction->operands[2];
        bound = runa_read_u32_le(instruction->operands + 3u);
        if (edge_bit(edge_or_level) == 0u ||
            (configuration->supported_edges & edge_bit(edge_or_level)) == 0u ||
            edge_or_level == RUNA_PULSE_EDGE_BOTH)
            return RUNA_ERR_INVALID_OPERAND;
        if (bound < configuration->minimum_timeout_us ||
            bound > configuration->maximum_timeout_us || bound > job->max_runtime_us)
            return RUNA_ERR_OUT_OF_RANGE;
    }
    return RUNA_OK;
}

static runa_status_t map_hal_status(runa_status_t status) {
    if (status == RUNA_OK || status == RUNA_ERR_IO_TIMEOUT ||
        status == RUNA_ERR_CANCELLED || status == RUNA_ERR_INTERNAL)
        return status;
    return RUNA_PULSE_ERR_IO;
}

static runa_status_t execute(void *context, runa_module_job_t *job,
                             const runa_module_instruction_t *instruction, uint32_t *detail) {
    runa_pulse_hal_t *hal = (runa_pulse_hal_t *)context;
    const runa_resource_t *resource;
    const runa_pulse_resource_config_t *configuration;
    uint8_t destination;
    uint32_t value = 0u;
    uint32_t bound;
    runa_status_t status;
    status = validate_common(job, instruction, &resource, &configuration, &destination, detail);
    if (status != RUNA_OK) return status;
    if (hal == NULL) return RUNA_ERR_INTERNAL;
    if (instruction->operation == RUNA_PULSE_OP_COUNT) {
        if (hal->count == NULL) return RUNA_ERR_INTERNAL;
        bound = runa_read_u32_le(instruction->operands + 3u);
        status = hal->count(hal->context, resource->platform_handle, instruction->operands[2],
                            bound, runa_read_u32_le(instruction->operands + 7u), &value);
        if (status == RUNA_OK && value > runa_read_u32_le(instruction->operands + 7u))
            status = RUNA_PULSE_ERR_INVALID_MEASUREMENT;
    } else if (instruction->operation == RUNA_PULSE_OP_MEASURE_WIDTH) {
        if (hal->measure_width == NULL) return RUNA_ERR_INTERNAL;
        bound = runa_read_u32_le(instruction->operands + 3u);
        status = hal->measure_width(hal->context, resource->platform_handle,
                                    instruction->operands[2], bound, &value);
        if (status == RUNA_OK && (value == 0u || value > bound))
            status = RUNA_PULSE_ERR_INVALID_MEASUREMENT;
    } else {
        if (hal->measure_period == NULL) return RUNA_ERR_INTERNAL;
        bound = runa_read_u32_le(instruction->operands + 3u);
        status = hal->measure_period(hal->context, resource->platform_handle,
                                     instruction->operands[2], bound, &value);
        if (status == RUNA_OK && (value == 0u || value > bound))
            status = RUNA_PULSE_ERR_INVALID_MEASUREMENT;
    }
    status = map_hal_status(status);
    if (status != RUNA_OK) {
        if (detail != NULL) *detail = resource->id;
        return status;
    }
    job->registers[destination] = value;
    return RUNA_OK;
}

static size_t capabilities(void *context, uint8_t *output, size_t capacity) {
    (void)context;
    if (output != NULL && capacity >= 22u) {
        output[0] = RUNA_PULSE_OP_COUNT;
        output[1] = RUNA_PULSE_OP_MEASURE_WIDTH;
        output[2] = RUNA_PULSE_OP_MEASURE_PERIOD;
        output[3] = 7u;
        output[4] = RUNA_PULSE_LEVEL_HIGH | RUNA_PULSE_LEVEL_LOW;
        output[5] = 0u;
        runa_write_u32_le(output + 6u, RUNA_PULSE_MAX_COUNT);
        runa_write_u32_le(output + 10u, RUNA_PULSE_MAX_WINDOW_US);
        runa_write_u32_le(output + 14u, RUNA_PULSE_MAX_TIMEOUT_US);
        runa_write_u32_le(output + 18u, 1u);
    }
    return 22u;
}

runa_module_t runa_pulse_module(runa_pulse_hal_t *hal) {
    runa_module_t module = { RUNA_PULSE_MODULE_ID, RUNA_MODULE_ABI_VERSION, 1u,
                             validate, execute, NULL, NULL, capabilities, hal,
                             NULL, 0u, validate_resource };
    return module;
}
