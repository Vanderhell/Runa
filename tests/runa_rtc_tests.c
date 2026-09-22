#include "runa_rtc.h"
#include "runa_capabilities.h"
#include "runa_ir.h"
#include "runa_limits.h"
#include "runa_registry.h"
#include "runa_runtime.h"
#include "runa_rtc_mock_hal.h"

#include <string.h>
#include <stdio.h>

typedef struct data_capture {
    uint32_t calls;
    uint16_t size;
    uint8_t data[RUNA_MAX_MODULE_DATA_BYTES];
} data_capture_t;

typedef struct event_capture {
    uint32_t calls;
    uint8_t data[8][RUNA_MAX_EVENT_BYTES];
    uint16_t sizes[8];
} event_capture_t;

typedef struct clock_fixture {
    uint64_t now;
} clock_fixture_t;

static runa_status_t emit_data(void *context, uint16_t module, uint16_t instruction,
                               uint16_t sequence, const uint8_t *data, size_t size) {
    data_capture_t *capture = (data_capture_t *)context;
    (void)module;
    (void)instruction;
    (void)sequence;
    if (capture == NULL || data == NULL || size > sizeof capture->data) return RUNA_ERR_INVALID_FORMAT;
    capture->size = (uint16_t)size;
    memcpy(capture->data, data, size);
    ++capture->calls;
    return RUNA_OK;
}

static int emit_event(void *context, const uint8_t *data, size_t size) {
    event_capture_t *capture = (event_capture_t *)context;
    if (capture == NULL || data == NULL || size > RUNA_MAX_EVENT_BYTES || capture->calls >= 8u)
        return -1;
    capture->sizes[capture->calls] = (uint16_t)size;
    memcpy(capture->data[capture->calls], data, size);
    ++capture->calls;
    return 0;
}

static uint64_t time_us(void *context) {
    return ((clock_fixture_t *)context)->now;
}

static void write_u64_le(uint8_t *data, uint64_t value) {
    uint8_t index;
    for (index = 0u; index < 8u; ++index) data[index] = (uint8_t)(value >> (index * 8u));
}

static runa_module_job_t make_job(const runa_resource_table_t *resources,
                                  data_capture_t *capture) {
    static uint32_t registers[RUNA_REGISTER_COUNT];
    runa_module_job_t job = { 0 };
    job.job_id = 1u;
    job.max_steps = RUNA_MAX_STEPS;
    job.max_runtime_us = RUNA_MAX_RUNTIME_US;
    job.max_result_bytes = RUNA_MAX_RESULT_BYTES;
    job.max_emits = RUNA_MAX_EMITS;
    job.max_emit_bytes = RUNA_MAX_EMIT_BYTES;
    job.registers = registers;
    job.register_count = RUNA_REGISTER_COUNT;
    job.resources = resources;
    job.emit_data = emit_data;
    job.emit_context = capture;
    return job;
}

static runa_module_instruction_t instruction(uint8_t operation, const uint8_t *operands,
                                             uint8_t operand_size) {
    runa_module_instruction_t value = { RUNA_RTC_MODULE_ID, operation, operand_size,
                                        operands, 0u, 0u };
    return value;
}

static size_t make_set_job(uint8_t *data, uint32_t id, uint64_t seconds, uint8_t invalid_later) {
    size_t offset = RUNA_HEADER_SIZE;
    size_t extension_size = 13u;
    size_t total;
    memset(data, 0, RUNA_MAX_JOB_BYTES);
    data[0] = 'J'; data[1] = 'E'; data[2] = 'X'; data[3] = 'E';
    data[4] = 1u; data[5] = RUNA_IR_VERSION_V2;
    runa_write_u16_le(data + 6u, RUNA_HEADER_SIZE);
    runa_write_u32_le(data + 8u, id);
    data[offset] = RUNA_OP_EXT;
    data[offset + 1u] = (uint8_t)extension_size;
    runa_write_u16_le(data + offset + 2u, RUNA_RTC_MODULE_ID);
    data[offset + 4u] = RUNA_RTC_OP_SET;
    runa_write_u16_le(data + offset + 5u, 13u);
    write_u64_le(data + offset + 7u, seconds);
    offset += 2u + extension_size;
    data[offset] = invalid_later == 0u ? RUNA_OP_RETURN : 0xffu;
    data[offset + 1u] = 1u;
    data[offset + 2u] = 0u;
    total = offset + 3u;
    runa_write_u32_le(data + 12u, (uint32_t)total);
    runa_write_u32_le(data + 16u, (uint32_t)(total - RUNA_HEADER_SIZE));
    runa_write_u16_le(data + 20u, 2u);
    runa_write_u32_le(data + 24u, 100u);
    runa_write_u32_le(data + 28u, 1000u);
    runa_write_u16_le(data + 32u, RUNA_MAX_RESULT_BYTES);
    runa_write_u16_le(data + 34u, RUNA_MAX_EMITS);
    runa_write_u32_le(data + 36u, RUNA_MAX_EMIT_BYTES);
    return total;
}

int main(void) {
    runa_rtc_mock_hal_t mock;
    runa_rtc_hal_t hal;
    runa_module_t module;
    runa_module_registry_t registry;
    data_capture_t capture = { 0 };
    uint8_t operands[10] = { 0 };
    uint32_t detail = 0u;
    runa_module_job_t job;
    runa_rtc_resource_config_t configuration = {
        0u, RUNA_RTC_MAX_SECONDS,
        RUNA_RTC_CAP_SET_SUPPORTED | RUNA_RTC_CAP_BATTERY_BACKED |
            RUNA_RTC_CAP_RETAINED_POWER_LOSS,
        RUNA_RTC_RUNTIME_STATUS_MASK
    };
    runa_resource_t resource = { 13u, RUNA_RTC_MODULE_ID, RUNA_RTC_RESOURCE_TYPE, 0u,
                                 RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE, 7u,
                                 &configuration };
    runa_resource_table_t resources = { &resource, 1u };
    uint8_t capability[256];
    size_t capability_size = 0u;
    runa_capabilities_view_t capability_view;
    uint8_t raw[RUNA_MAX_JOB_BYTES];
    event_capture_t events = { 0 };
    clock_fixture_t clock = { 100u };
    runa_platform_t platform = { &clock, time_us, NULL };
    runa_event_sink_t sink = { emit_event, &events };
    runa_execution_summary_t summary;
    uint32_t checks = 0u;
#define CHECK(value) do { ++checks; if (!(value)) return (int)checks; } while (0)

    runa_rtc_mock_hal_init(&mock);
    mock.current_time.seconds = UINT64_C(2147483648);
    mock.status_flags = RUNA_RTC_STATUS_VALID_TIME;
    hal = runa_rtc_mock_hal_interface(&mock);
    module = runa_rtc_module(&hal);
    runa_registry_init(&registry);
    CHECK(runa_registry_add(&registry, &module) == RUNA_OK);
    CHECK(runa_resource_table_validate(&resources, &registry) == RUNA_OK);
    job = make_job(&resources, &capture);

    runa_write_u16_le(operands, RUNA_RTC_MODULE_ID);
    {
        runa_module_instruction_t current = instruction(RUNA_RTC_OP_READ, operands, 2u);
        CHECK(module.validate(module.context, &job, &current, &detail) == RUNA_OK);
        CHECK(module.execute(module.context, &job, &current, &detail) == RUNA_OK);
        CHECK(capture.calls == 1u && capture.size == RUNA_RTC_RESULT_TIME_BYTES &&
              capture.data[0] == RUNA_RTC_RESULT_VERSION && capture.data[1] == RUNA_RTC_RESULT_READ &&
              capture.data[7] == 128u && capture.data[12] == 25u);
    }
    {
        runa_module_instruction_t current = instruction(RUNA_RTC_OP_GET_STATUS, operands, 2u);
        mock.status_flags = RUNA_RTC_STATUS_VALID_TIME | RUNA_RTC_STATUS_POWER_LOSS_DETECTED;
        CHECK(module.execute(module.context, &job, &current, &detail) == RUNA_OK);
        CHECK(capture.size == RUNA_RTC_RESULT_STATUS_BYTES && capture.data[1] == RUNA_RTC_RESULT_STATUS &&
              capture.data[4] == 27u);
    }
    write_u64_le(operands + 2u, RUNA_RTC_MAX_SECONDS);
    {
        runa_module_instruction_t current = instruction(RUNA_RTC_OP_SET, operands, 10u);
        CHECK(module.validate(module.context, &job, &current, &detail) == RUNA_OK);
        CHECK(module.execute(module.context, &job, &current, &detail) == RUNA_OK &&
              mock.last_set_time.seconds == RUNA_RTC_MAX_SECONDS);
        current.operand_size = 9u;
        CHECK(module.validate(module.context, &job, &current, &detail) == RUNA_ERR_INVALID_OPERAND &&
              mock.set_calls == 1u);
    }
    write_u64_le(operands + 2u, RUNA_RTC_MAX_SECONDS + 1u);
    {
        runa_module_instruction_t current = instruction(RUNA_RTC_OP_SET, operands, 10u);
        CHECK(module.validate(module.context, &job, &current, &detail) == RUNA_ERR_OUT_OF_RANGE &&
              mock.set_calls == 1u);
        resource.permissions = RUNA_PERMISSION_READ;
        CHECK(module.validate(module.context, &job, &current, &detail) == RUNA_ERR_ACCESS_DENIED);
        resource.permissions = RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE;
    }
    configuration.capability_flags &= (uint32_t)~RUNA_RTC_CAP_SET_SUPPORTED;
    {
        runa_module_instruction_t current = instruction(RUNA_RTC_OP_SET, operands, 10u);
        CHECK(module.validate(module.context, &job, &current, &detail) == RUNA_RTC_ERR_SET_UNSUPPORTED &&
              mock.set_calls == 1u);
    }
    configuration.capability_flags |= RUNA_RTC_CAP_SET_SUPPORTED;
    mock.status_flags = 0x80u;
    {
        runa_module_instruction_t current = instruction(RUNA_RTC_OP_GET_STATUS, operands, 2u);
        CHECK(module.execute(module.context, &job, &current, &detail) == RUNA_RTC_ERR_STATUS_FLAGS);
    }
    mock.status_flags = RUNA_RTC_STATUS_VALID_TIME;
    mock.next_read_status = RUNA_ERR_IO_TIMEOUT;
    {
        runa_module_instruction_t current = instruction(RUNA_RTC_OP_READ, operands, 2u);
        CHECK(module.execute(module.context, &job, &current, &detail) == RUNA_ERR_IO_TIMEOUT);
    }
    CHECK(runa_capabilities_encode(&registry, capability, sizeof capability, &capability_size) == RUNA_OK);
    CHECK(runa_capabilities_decode(capability, capability_size, &capability_view) == RUNA_OK &&
          capability_view.module_count == 1u && capability_view.modules[0].module_id == RUNA_RTC_MODULE_ID &&
          capability_view.modules[0].payload_size == RUNA_RTC_CAPABILITY_PAYLOAD_SIZE &&
          capability_view.modules[0].payload[2] == 7u && capability_view.modules[0].payload[19] == 0u);

    mock.current_time.seconds = 1u;
    events.calls = 0u;
    (void)make_set_job(raw, 1u, RUNA_RTC_MAX_SECONDS, 0u);
    summary = runa_process(raw, runa_read_u32_le(raw + 12u), &resources, &registry, &platform, &sink);
    CHECK(summary.accepted == 1u && summary.result_sent == 1u && summary.error == RUNA_OK &&
          events.calls == 2u && mock.current_time.seconds == RUNA_RTC_MAX_SECONDS && clock.now == 100u);
    (void)make_set_job(raw, 2u, RUNA_RTC_MIN_SECONDS, 0u);
    summary = runa_process(raw, runa_read_u32_le(raw + 12u), &resources, &registry, &platform, &sink);
    CHECK(summary.accepted == 1u && summary.result_sent == 1u && mock.current_time.seconds == 0u &&
          clock.now == 100u);
    (void)make_set_job(raw, 3u, 42u, 1u);
    summary = runa_process(raw, runa_read_u32_le(raw + 12u), &resources, &registry, &platform, &sink);
    CHECK(summary.accepted == 0u && summary.result_sent == 1u && mock.set_calls == 3u &&
          mock.current_time.seconds == 0u && clock.now == 100u);
    mock.set_calls = 0u;
    for (uint32_t case_index = 0u; case_index < 1000u; ++case_index) {
        uint64_t candidate = case_index % 2u == 0u ? RUNA_RTC_MAX_SECONDS : case_index;
        (void)make_set_job(raw, 0x5000u + case_index, candidate, 1u);
        summary = runa_process(raw, runa_read_u32_le(raw + 12u), &resources, &registry, &platform, &sink);
        CHECK(summary.error != RUNA_OK && mock.set_calls == 0u);
    }
    return 0;
#undef CHECK
}
