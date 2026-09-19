#include "runa_rtc.h"

#include "runa_ir.h"
#include "runa_resource.h"

static uint64_t read_u64_le(const uint8_t *data) {
    uint64_t value = 0u;
    uint8_t index;
    for (index = 0u; index < 8u; ++index)
        value |= (uint64_t)data[index] << (index * 8u);
    return value;
}

static void write_u64_le(uint8_t *data, uint64_t value) {
    uint8_t index;
    for (index = 0u; index < 8u; ++index)
        data[index] = (uint8_t)(value >> (index * 8u));
}

static const runa_rtc_resource_config_t *configuration_for(const runa_resource_t *resource) {
    return resource == NULL ? NULL : (const runa_rtc_resource_config_t *)resource->config;
}

static runa_status_t validate_configuration(const runa_rtc_resource_config_t *configuration) {
    if (configuration == NULL || configuration->minimum_seconds > configuration->maximum_seconds ||
        configuration->maximum_seconds > RUNA_RTC_MAX_SECONDS ||
        (configuration->capability_flags & (uint32_t)~RUNA_RTC_CAPABILITY_FLAGS_MASK) != 0u ||
        (configuration->supported_status_flags & (uint32_t)~RUNA_RTC_RUNTIME_STATUS_MASK) != 0u)
        return RUNA_ERR_INVALID_RESOURCE;
    if ((configuration->capability_flags & RUNA_RTC_CAP_RETAINED_POWER_LOSS) != 0u &&
        (configuration->capability_flags & RUNA_RTC_CAP_BATTERY_BACKED) == 0u)
        return RUNA_ERR_INVALID_RESOURCE;
    return RUNA_OK;
}

static runa_status_t validate_resource(void *context, const runa_resource_t *resource,
                                       uint32_t *detail) {
    (void)context;
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (detail != NULL) *detail = resource->id;
    if (resource->resource_type != RUNA_RTC_RESOURCE_TYPE) return RUNA_ERR_RESOURCE_TYPE;
    return validate_configuration(configuration_for(resource));
}

static runa_status_t validate_status_flags(const runa_rtc_resource_config_t *configuration,
                                            uint32_t flags) {
    if ((flags & (uint32_t)~RUNA_RTC_RUNTIME_STATUS_MASK) != 0u ||
        (flags & ~configuration->supported_status_flags) != 0u)
        return RUNA_RTC_ERR_STATUS_FLAGS;
    return RUNA_OK;
}

static uint32_t effective_status_flags(const runa_rtc_resource_config_t *configuration,
                                       uint32_t flags) {
    if ((configuration->capability_flags & RUNA_RTC_CAP_BATTERY_BACKED) != 0u)
        flags |= RUNA_RTC_STATUS_BATTERY_BACKED;
    if ((configuration->capability_flags & RUNA_RTC_CAP_SET_SUPPORTED) != 0u)
        flags |= RUNA_RTC_STATUS_SET_SUPPORTED;
    return flags;
}

static int operation_available(const runa_rtc_hal_t *hal, uint8_t operation) {
    if (hal == NULL) return 0;
    if (operation == RUNA_RTC_OP_READ) return hal->read != NULL;
    if (operation == RUNA_RTC_OP_SET) return hal->set != NULL;
    if (operation == RUNA_RTC_OP_GET_STATUS) return hal->status != NULL;
    return 0;
}

static runa_status_t validate(void *context, const runa_module_job_t *job,
                              const runa_module_instruction_t *instruction,
                              uint32_t *detail) {
    const runa_rtc_hal_t *hal = (const runa_rtc_hal_t *)context;
    const runa_resource_t *resource;
    const runa_rtc_resource_config_t *configuration;
    uint16_t resource_id;
    uint32_t permission;
    runa_status_t status;

    if (instruction == NULL || instruction->operands == NULL)
        return RUNA_ERR_INVALID_OPERAND;
    if (instruction->operation == RUNA_RTC_OP_READ ||
        instruction->operation == RUNA_RTC_OP_GET_STATUS) {
        if (instruction->operand_size != 2u) return RUNA_ERR_INVALID_OPERAND;
        permission = RUNA_PERMISSION_READ;
    } else if (instruction->operation == RUNA_RTC_OP_SET) {
        if (instruction->operand_size != 10u) return RUNA_ERR_INVALID_OPERAND;
        permission = RUNA_PERMISSION_WRITE;
    } else {
        return RUNA_ERR_INVALID_OPCODE;
    }
    if (job == NULL) return RUNA_ERR_INVALID_FORMAT;

    resource_id = runa_read_u16_le(instruction->operands);
    if (detail != NULL) *detail = resource_id;
    resource = runa_resource_find(job->resources, resource_id);
    if (resource == NULL) return RUNA_ERR_INVALID_RESOURCE;
    if (resource->module_id != RUNA_RTC_MODULE_ID ||
        resource->resource_type != RUNA_RTC_RESOURCE_TYPE)
        return RUNA_ERR_RESOURCE_TYPE;
    if ((resource->permissions & permission) != permission)
        return RUNA_ERR_ACCESS_DENIED;
    status = validate_resource(context, resource, detail);
    if (status != RUNA_OK) return status;
    configuration = configuration_for(resource);

    if (instruction->operation == RUNA_RTC_OP_SET) {
        runa_rtc_time_t time;
        if ((configuration->capability_flags & RUNA_RTC_CAP_SET_SUPPORTED) == 0u ||
            operation_available(hal, RUNA_RTC_OP_SET) == 0)
            return RUNA_RTC_ERR_SET_UNSUPPORTED;
        time.seconds = read_u64_le(instruction->operands + 2u);
        if (time.seconds < configuration->minimum_seconds ||
            time.seconds > configuration->maximum_seconds)
            return RUNA_ERR_OUT_OF_RANGE;
    } else if (operation_available(hal, instruction->operation) == 0) {
        return RUNA_RTC_ERR_UNSUPPORTED;
    }
    return RUNA_OK;
}

static runa_status_t read_status(const runa_rtc_hal_t *hal, const runa_resource_t *resource,
                                  const runa_rtc_resource_config_t *configuration,
                                  uint32_t *flags) {
    runa_status_t status;
    if (hal == NULL || hal->status == NULL || flags == NULL) return RUNA_RTC_ERR_UNSUPPORTED;
    status = hal->status(hal->context, resource->platform_handle, flags);
    if (status != RUNA_OK) return status;
    status = validate_status_flags(configuration, *flags);
    if (status != RUNA_OK) return status;
    *flags = effective_status_flags(configuration, *flags);
    return RUNA_OK;
}

static runa_status_t emit_data(runa_module_job_t *job,
                               const runa_module_instruction_t *instruction,
                               const uint8_t *data, size_t size) {
    if (job == NULL || instruction == NULL || job->emit_data == NULL)
        return RUNA_ERR_INTERNAL;
    return job->emit_data(job->emit_context, RUNA_RTC_MODULE_ID,
                          instruction->instruction_index, 0u, data, size);
}

static runa_status_t emit_time(runa_module_job_t *job,
                               const runa_module_instruction_t *instruction,
                               const runa_rtc_time_t *time, uint32_t flags) {
    uint8_t payload[RUNA_RTC_RESULT_TIME_BYTES] = { 0u };
    payload[0] = RUNA_RTC_RESULT_VERSION;
    payload[1] = RUNA_RTC_RESULT_READ;
    write_u64_le(payload + 4u, time->seconds);
    runa_write_u32_le(payload + 12u, flags);
    return emit_data(job, instruction, payload, sizeof payload);
}

static runa_status_t emit_status(runa_module_job_t *job,
                                 const runa_module_instruction_t *instruction,
                                 uint32_t flags) {
    uint8_t payload[RUNA_RTC_RESULT_STATUS_BYTES] = { 0u };
    payload[0] = RUNA_RTC_RESULT_VERSION;
    payload[1] = RUNA_RTC_RESULT_STATUS;
    runa_write_u32_le(payload + 4u, flags);
    return emit_data(job, instruction, payload, sizeof payload);
}

static runa_status_t execute(void *context, runa_module_job_t *job,
                             const runa_module_instruction_t *instruction,
                             uint32_t *detail) {
    runa_rtc_hal_t *hal = (runa_rtc_hal_t *)context;
    const runa_resource_t *resource;
    const runa_rtc_resource_config_t *configuration;
    uint16_t resource_id;
    runa_status_t status;

    if (hal == NULL || job == NULL || instruction == NULL || instruction->operands == NULL)
        return RUNA_ERR_INTERNAL;
    resource_id = runa_read_u16_le(instruction->operands);
    if (detail != NULL) *detail = resource_id;
    resource = runa_resource_find(job->resources, resource_id);
    configuration = configuration_for(resource);
    if (resource == NULL || configuration == NULL) return RUNA_ERR_INTERNAL;

    if (instruction->operation == RUNA_RTC_OP_SET) {
        runa_rtc_time_t time;
        time.seconds = read_u64_le(instruction->operands + 2u);
        if (hal->set == NULL) return RUNA_RTC_ERR_SET_UNSUPPORTED;
        return hal->set(hal->context, resource->platform_handle, &time);
    }
    if (instruction->operation == RUNA_RTC_OP_GET_STATUS) {
        uint32_t flags = 0u;
        status = read_status(hal, resource, configuration, &flags);
        return status == RUNA_OK ? emit_status(job, instruction, flags) : status;
    }
    if (instruction->operation == RUNA_RTC_OP_READ) {
        runa_rtc_time_t time = { 0u };
        uint32_t flags = 0u;
        if (hal->read == NULL) return RUNA_RTC_ERR_UNSUPPORTED;
        status = hal->read(hal->context, resource->platform_handle, &time);
        if (status != RUNA_OK) return status;
        if (time.seconds < configuration->minimum_seconds ||
            time.seconds > configuration->maximum_seconds)
            return RUNA_ERR_OUT_OF_RANGE;
        status = read_status(hal, resource, configuration, &flags);
        return status == RUNA_OK ? emit_time(job, instruction, &time, flags) : status;
    }
    return RUNA_ERR_INVALID_OPCODE;
}

static size_t capabilities(void *context, uint8_t *output, size_t capacity) {
    const runa_rtc_hal_t *hal = (const runa_rtc_hal_t *)context;
    uint8_t operations = 0u;
    if (hal != NULL) {
        if (hal->read != NULL) operations |= 1u << (RUNA_RTC_OP_READ - 1u);
        if (hal->set != NULL) operations |= 1u << (RUNA_RTC_OP_SET - 1u);
        if (hal->status != NULL) operations |= 1u << (RUNA_RTC_OP_GET_STATUS - 1u);
    }
    if (output != NULL && capacity >= RUNA_RTC_CAPABILITY_PAYLOAD_SIZE) {
        output[0] = RUNA_RTC_RESULT_VERSION;
        output[1] = 1u; /* resolution: one whole second */
        output[2] = operations;
        output[3] = 1u; /* UTC/POSIX seconds representation */
        write_u64_le(output + 4u, RUNA_RTC_MIN_SECONDS);
        write_u64_le(output + 12u, RUNA_RTC_MAX_SECONDS);
    }
    return RUNA_RTC_CAPABILITY_PAYLOAD_SIZE;
}

runa_module_t runa_rtc_module(runa_rtc_hal_t *hal) {
    runa_module_t module = { RUNA_RTC_MODULE_ID, RUNA_MODULE_ABI_VERSION, 1u,
                             validate, execute, NULL, NULL, capabilities, hal,
                             NULL, 0u, validate_resource };
    return module;
}
