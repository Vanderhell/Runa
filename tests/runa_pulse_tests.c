#include "runa_pulse.h"
#include "runa_capabilities.h"
#include "runa_ir.h"
#include "runa_runtime.h"
#include "runa_pulse_mock_hal.h"

#include <stdio.h>
#include <string.h>

typedef struct fixture {
    uint8_t events[4][RUNA_MAX_EVENT_BYTES];
    size_t sizes[4];
    uint8_t event_count;
} fixture_t;

static int capture(void *context, const uint8_t *event, size_t size) {
    fixture_t *fixture = (fixture_t *)context;
    if (fixture == NULL || event == NULL || fixture->event_count >= 4u ||
        size > RUNA_MAX_EVENT_BYTES) return -1;
    memcpy(fixture->events[fixture->event_count], event, size);
    fixture->sizes[fixture->event_count] = size;
    ++fixture->event_count;
    return 0;
}

static uint64_t time_us(void *context) { (void)context; return 0u; }

static size_t make_job(uint8_t *job, uint8_t operation, uint16_t resource_id,
                       uint8_t selector, uint32_t bound, uint32_t max_count,
                       uint8_t destination) {
    uint8_t *instruction;
    uint8_t payload_size = operation == RUNA_PULSE_OP_COUNT ? 12u : 8u;
    uint8_t operand_size = (uint8_t)(3u + payload_size);
    uint32_t instruction_bytes = (uint32_t)(2u + operand_size + 3u);
    memset(job, 0, RUNA_HEADER_SIZE + instruction_bytes);
    job[0] = 'J'; job[1] = 'E'; job[2] = 'X'; job[3] = 'E';
    job[4] = RUNA_PROTOCOL_VERSION; job[5] = RUNA_IR_VERSION_V2;
    runa_write_u16_le(job + 6u, RUNA_HEADER_SIZE);
    runa_write_u32_le(job + 8u, 0x12345678u);
    runa_write_u32_le(job + 12u, RUNA_HEADER_SIZE + instruction_bytes);
    runa_write_u32_le(job + 16u, instruction_bytes);
    runa_write_u16_le(job + 20u, 2u);
    runa_write_u32_le(job + 24u, 20u);
    runa_write_u32_le(job + 28u, 1000u);
    runa_write_u16_le(job + 32u, RUNA_MAX_RESULT_BYTES);
    runa_write_u16_le(job + 34u, 8u);
    runa_write_u32_le(job + 36u, RUNA_MAX_EMIT_BYTES);
    instruction = job + RUNA_HEADER_SIZE;
    instruction[0] = RUNA_OP_EXT;
    instruction[1] = operand_size;
    runa_write_u16_le(instruction + 2u, RUNA_PULSE_MODULE_ID);
    instruction[4] = operation;
    runa_write_u16_le(instruction + 5u, resource_id);
    instruction[7] = selector;
    runa_write_u32_le(instruction + 8u, bound);
    if (operation == RUNA_PULSE_OP_COUNT) runa_write_u32_le(instruction + 12u, max_count);
    instruction[5u + payload_size - 1u] = destination;
    instruction += 2u + operand_size;
    instruction[0] = RUNA_OP_RETURN; instruction[1] = 1u;
    instruction[2] = (uint8_t)(1u << destination);
    return RUNA_HEADER_SIZE + instruction_bytes;
}

static int expect_error(fixture_t *fixture, runa_pulse_mock_state_t *mock,
                        const runa_resource_table_t *resources,
                        const runa_module_registry_t *registry, const uint8_t *job,
                        size_t size, runa_status_t expected) {
    runa_platform_t platform = { fixture, time_us, NULL };
    runa_event_sink_t sink = { capture, fixture };
    runa_execution_summary_t result = runa_process(job, size, resources, registry,
                                                   &platform, &sink);
    return result.error == expected && fixture->event_count == 1u &&
           fixture->events[0][0] == RUNA_EVENT_RESULT && mock->count_calls == 0u &&
           mock->width_calls == 0u && mock->period_calls == 0u;
}

int main(void) {
    fixture_t fixture = {0};
    runa_pulse_mock_state_t mock = {0};
    runa_pulse_hal_t hal = runa_pulse_mock_hal(&mock);
    runa_module_t pulse = runa_pulse_module(&hal);
    runa_module_registry_t registry;
    runa_pulse_resource_config_t configuration = { 1u, 1000u, 1u, 1000u, 100u, 7u, 3u, 0u };
    runa_resource_t resource = { 8u, RUNA_PULSE_MODULE_ID, RUNA_PULSE_RESOURCE_TYPE, 0u,
                                 RUNA_PERMISSION_READ, (uintptr_t)0x1234u, &configuration };
    runa_resource_table_t resources = { &resource, 1u };
    runa_platform_t platform = { &fixture, time_us, NULL };
    runa_event_sink_t sink = { capture, &fixture };
    runa_execution_summary_t result;
    uint8_t job[RUNA_MAX_JOB_BYTES];
    uint8_t capabilities_bytes[128];
    size_t capabilities_size;
    runa_capabilities_view_t capability_view;

    runa_registry_init(&registry);
    if (runa_registry_add(&registry, &pulse) != RUNA_OK ||
        runa_capabilities_encode(&registry, capabilities_bytes, sizeof capabilities_bytes,
                                 &capabilities_size) != RUNA_OK ||
        runa_capabilities_decode(capabilities_bytes, capabilities_size, &capability_view) != RUNA_OK ||
        capability_view.module_count != 1u || capability_view.modules[0].module_id != RUNA_PULSE_MODULE_ID ||
        capability_view.modules[0].payload_size != 22u ||
        capability_view.modules[0].payload[0] != RUNA_PULSE_OP_COUNT ||
        capability_view.modules[0].payload[3] != 7u) return 1;

    mock.count_value = 7u;
    result = runa_process(job, make_job(job, RUNA_PULSE_OP_COUNT, 8u,
                                        RUNA_PULSE_EDGE_RISING, 100u, 100u, 0u),
                          &resources, &registry, &platform, &sink);
    if (result.error != RUNA_OK || mock.count_calls != 1u || mock.last_handle != (uintptr_t)0x1234u ||
        mock.last_edge != RUNA_PULSE_EDGE_RISING || mock.last_bound != 100u ||
        mock.last_max_count != 100u || fixture.event_count != 2u ||
        fixture.events[0][0] != RUNA_EVENT_ACK || fixture.events[1][0] != RUNA_EVENT_RESULT ||
        runa_read_u32_le(fixture.events[1] + 20u) != 7u) {
        return 2;
    }

    memset(&fixture, 0, sizeof fixture);
    mock.width_value = 42u;
    result = runa_process(job, make_job(job, RUNA_PULSE_OP_MEASURE_WIDTH, 8u,
                                        RUNA_PULSE_LEVEL_HIGH, 100u, 0u, 1u),
                          &resources, &registry, &platform, &sink);
    if (result.error != RUNA_OK || mock.width_calls != 1u || mock.last_level != RUNA_PULSE_LEVEL_HIGH ||
        runa_read_u32_le(fixture.events[1] + 20u) != 42u) return 3;

    memset(&fixture, 0, sizeof fixture);
    mock.period_value = 80u;
    result = runa_process(job, make_job(job, RUNA_PULSE_OP_MEASURE_PERIOD, 8u,
                                        RUNA_PULSE_EDGE_FALLING, 100u, 0u, 2u),
                          &resources, &registry, &platform, &sink);
    if (result.error != RUNA_OK || mock.period_calls != 1u || mock.last_edge != RUNA_PULSE_EDGE_FALLING ||
        runa_read_u32_le(fixture.events[1] + 20u) != 80u) return 4;

    memset(&mock, 0, sizeof mock);
    memset(&fixture, 0, sizeof fixture);
    if (!expect_error(&fixture, &mock, &resources, &registry,
                      job, make_job(job, RUNA_PULSE_OP_COUNT, 8u, 99u, 100u, 100u, 0u),
                      RUNA_ERR_INVALID_OPERAND)) return 5;
    memset(&fixture, 0, sizeof fixture);
    if (!expect_error(&fixture, &mock, &resources, &registry,
                      job, make_job(job, RUNA_PULSE_OP_COUNT, 8u, RUNA_PULSE_EDGE_RISING,
                                    1001u, 100u, 0u), RUNA_ERR_OUT_OF_RANGE)) return 6;
    memset(&fixture, 0, sizeof fixture);
    if (!expect_error(&fixture, &mock, &resources, &registry,
                      job, make_job(job, RUNA_PULSE_OP_COUNT, 8u, RUNA_PULSE_EDGE_RISING,
                                    100u, 101u, 0u), RUNA_ERR_OUT_OF_RANGE)) return 7;
    memset(&fixture, 0, sizeof fixture);
    resource.permissions = 0u;
    if (!expect_error(&fixture, &mock, &resources, &registry,
                      job, make_job(job, RUNA_PULSE_OP_MEASURE_WIDTH, 8u,
                                    RUNA_PULSE_LEVEL_HIGH, 100u, 0u, 0u),
                      RUNA_ERR_ACCESS_DENIED)) return 8;
    resource.permissions = RUNA_PERMISSION_READ;
    memset(&fixture, 0, sizeof fixture);
    mock.count_status = RUNA_ERR_IO_TIMEOUT;
    result = runa_process(job, make_job(job, RUNA_PULSE_OP_COUNT, 8u,
                                        RUNA_PULSE_EDGE_BOTH, 100u, 100u, 0u),
                          &resources, &registry, &platform, &sink);
    if (result.error != RUNA_ERR_IO_TIMEOUT || fixture.event_count != 2u || mock.count_calls != 1u)
        return 9;
    mock.count_status = RUNA_OK;
    mock.count_value = 1u;
    memset(&fixture, 0, sizeof fixture);
    result = runa_process(job, make_job(job, RUNA_PULSE_OP_COUNT, 8u,
                                        RUNA_PULSE_EDGE_RISING, 1u, 1u, 0u),
                          &resources, &registry, &platform, &sink);
    if (result.error != RUNA_OK || fixture.event_count != 2u || mock.count_calls != 2u) {
        return 10;
    }

    puts("Runa.Pulse bounded measurement checks passed");
    return 0;
}
