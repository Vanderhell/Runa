#include "runa_watchdog.h"
#include "runa_watchdog_mock_hal.h"

#include "runa_capabilities.h"
#include "runa_ir.h"
#include "runa_platform.h"
#include "runa_registry.h"
#include "runa_resource.h"
#include "runa_result.h"
#include "runa_runtime.h"

#include <stdio.h>
#include <string.h>

typedef struct fixture {
    uint8_t events[8][RUNA_MAX_EVENT_BYTES];
    size_t sizes[8];
    uint8_t event_count;
    uint64_t now_us;
} fixture_t;

static int capture(void *context, const uint8_t *data, size_t size) {
    fixture_t *fixture = (fixture_t *)context;
    if (fixture == NULL || data == NULL || size > RUNA_MAX_EVENT_BYTES ||
        fixture->event_count >= 8u) return -1;
    memcpy(fixture->events[fixture->event_count], data, size);
    fixture->sizes[fixture->event_count] = size;
    ++fixture->event_count;
    return 0;
}

static uint64_t time_us(void *context) {
    return ((fixture_t *)context)->now_us;
}

static size_t make_job(uint8_t *job, uint8_t operation, const uint8_t *payload,
                       uint8_t payload_size) {
    uint8_t *instruction = job + RUNA_HEADER_SIZE;
    uint8_t operand_size = (uint8_t)(3u + payload_size);
    uint32_t instruction_bytes = (uint32_t)(2u + operand_size + 3u);
    memset(job, 0, RUNA_MAX_JOB_BYTES);
    job[0] = 'J'; job[1] = 'E'; job[2] = 'X'; job[3] = 'E';
    job[4] = RUNA_PROTOCOL_VERSION; job[5] = RUNA_IR_VERSION_V2;
    runa_write_u16_le(job + 6u, RUNA_HEADER_SIZE);
    runa_write_u32_le(job + 8u, 0x12345678u);
    runa_write_u32_le(job + 12u, RUNA_HEADER_SIZE + instruction_bytes);
    runa_write_u32_le(job + 16u, instruction_bytes);
    runa_write_u16_le(job + 20u, 2u);
    runa_write_u32_le(job + 24u, 100u);
    runa_write_u32_le(job + 28u, 100000u);
    runa_write_u16_le(job + 32u, RUNA_MAX_RESULT_BYTES);
    runa_write_u16_le(job + 34u, 8u);
    runa_write_u32_le(job + 36u, RUNA_MAX_EMIT_BYTES);
    instruction[0] = RUNA_OP_EXT;
    instruction[1] = operand_size;
    runa_write_u16_le(instruction + 2u, RUNA_WATCHDOG_MODULE_ID);
    instruction[4] = operation;
    memcpy(instruction + 5u, payload, payload_size);
    instruction += 2u + operand_size;
    instruction[0] = RUNA_OP_RETURN;
    instruction[1] = 1u;
    instruction[2] = 0u;
    return RUNA_HEADER_SIZE + instruction_bytes;
}

static size_t make_malformed_after_arm(uint8_t *job) {
    uint8_t *instruction = job + RUNA_HEADER_SIZE;
    uint32_t instruction_bytes = (uint32_t)((2u + 9u) + (2u + 2u) + 3u);
    memset(job, 0, RUNA_MAX_JOB_BYTES);
    job[0] = 'J'; job[1] = 'E'; job[2] = 'X'; job[3] = 'E';
    job[4] = RUNA_PROTOCOL_VERSION; job[5] = RUNA_IR_VERSION_V2;
    runa_write_u16_le(job + 6u, RUNA_HEADER_SIZE);
    runa_write_u32_le(job + 8u, 0x87654321u);
    runa_write_u32_le(job + 12u, RUNA_HEADER_SIZE + instruction_bytes);
    runa_write_u32_le(job + 16u, instruction_bytes);
    runa_write_u16_le(job + 20u, 3u);
    runa_write_u32_le(job + 24u, 100u);
    runa_write_u32_le(job + 28u, 100000u);
    runa_write_u16_le(job + 32u, RUNA_MAX_RESULT_BYTES);
    runa_write_u16_le(job + 34u, 8u);
    runa_write_u32_le(job + 36u, RUNA_MAX_EMIT_BYTES);
    instruction[0] = RUNA_OP_EXT; instruction[1] = 9u;
    runa_write_u16_le(instruction + 2u, RUNA_WATCHDOG_MODULE_ID); instruction[4] = RUNA_WATCHDOG_OP_ARM;
    runa_write_u16_le(instruction + 5u, 14u); runa_write_u32_le(instruction + 7u, 1000u);
    instruction += 11u;
    instruction[0] = RUNA_OP_EXT; instruction[1] = 2u;
    runa_write_u16_le(instruction + 2u, RUNA_WATCHDOG_MODULE_ID); instruction[4] = 0xffu;
    instruction += 4u;
    instruction[0] = RUNA_OP_RETURN; instruction[1] = 1u; instruction[2] = 0u;
    return RUNA_HEADER_SIZE + instruction_bytes;
}

static runa_execution_summary_t run_job(fixture_t *fixture,
                                        runa_watchdog_mock_state_t *mock,
                                        runa_resource_t *resource, uint8_t *job,
                                        size_t size, runa_module_registry_t *registry) {
    runa_platform_t platform = { fixture, time_us, NULL };
    runa_event_sink_t sink = { capture, fixture };
    runa_resource_table_t resources = { resource, 1u };
    runa_execution_summary_t summary;
    fixture->event_count = 0u;
    (void)mock;
    summary = runa_process(job, size, &resources, registry, &platform, &sink);
    return summary;
}

int main(void) {
    runa_watchdog_mock_state_t mock;
    runa_watchdog_hal_t hal;
    runa_module_t module;
    runa_module_registry_t registry;
    runa_watchdog_resource_config_t configuration = {
        100u, 10000u, 1000u,
        RUNA_WATCHDOG_SUPPORT_STATUS | RUNA_WATCHDOG_SUPPORT_ARM |
            RUNA_WATCHDOG_SUPPORT_FEED | RUNA_WATCHDOG_SUPPORT_DISARM,
        0u, 1u, 0u
    };
    runa_resource_t resource = { 14u, RUNA_WATCHDOG_MODULE_ID, RUNA_WATCHDOG_RESOURCE_TYPE,
                                 0u, RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE,
                                 (uintptr_t)0x55u, &configuration };
    fixture_t fixture = {0};
    uint8_t job[RUNA_MAX_JOB_BYTES];
    uint8_t payload[6];
    uint8_t capability_bytes[RUNA_MAX_CAPABILITY_BYTES];
    size_t capability_size;
    runa_capabilities_view_t capability_view;
    runa_execution_summary_t summary;

    runa_watchdog_mock_init(&mock);
    hal = runa_watchdog_mock_hal(&mock);
    module = runa_watchdog_module(&hal);
    runa_registry_init(&registry);
    if (runa_registry_add(&registry, &module) != RUNA_OK ||
        runa_capabilities_encode(&registry, capability_bytes, sizeof capability_bytes,
                                 &capability_size) != RUNA_OK ||
        runa_capabilities_decode(capability_bytes, capability_size, &capability_view) != RUNA_OK ||
        capability_view.module_count != 1u || capability_view.modules[0].module_id != 14u ||
        capability_view.modules[0].payload_size != RUNA_WATCHDOG_CAPABILITY_SIZE ||
        capability_view.modules[0].payload[0] != RUNA_WATCHDOG_CAPABILITY_VERSION ||
        capability_view.modules[0].payload[1] != 0x0fu) return 1;

    runa_write_u16_le(payload, resource.id);
    summary = run_job(&fixture, &mock, &resource, job,
                      make_job(job, RUNA_WATCHDOG_OP_GET_STATUS, payload, 2u), &registry);
    if (summary.error != RUNA_OK || fixture.event_count != 3u || mock.status_calls != 1u ||
        fixture.events[0][0] != RUNA_EVENT_ACK || fixture.events[1][0] != RUNA_EVENT_MODULE_DATA ||
        fixture.events[2][0] != RUNA_EVENT_RESULT || fixture.sizes[1] != 24u ||
        fixture.events[1][16] != RUNA_WATCHDOG_STATUS_DATA_VERSION ||
        (fixture.events[1][17] & (RUNA_WATCHDOG_STATUS_SUPPORTED | RUNA_WATCHDOG_STATUS_DISARM_SUPPORTED)) !=
            (RUNA_WATCHDOG_STATUS_SUPPORTED | RUNA_WATCHDOG_STATUS_DISARM_SUPPORTED)) return 2;

    runa_write_u32_le(payload + 2u, 1000u);
    summary = run_job(&fixture, &mock, &resource, job,
                      make_job(job, RUNA_WATCHDOG_OP_ARM, payload, 6u), &registry);
    if (summary.error != RUNA_OK || mock.arm_calls != 1u || mock.armed == 0u ||
        mock.timeout_ms != 1000u || fixture.event_count != 2u) return 3;

    summary = run_job(&fixture, &mock, &resource, job,
                      make_job(job, RUNA_WATCHDOG_OP_GET_STATUS, payload, 2u), &registry);
    if (summary.error != RUNA_OK || mock.armed == 0u || fixture.event_count != 3u ||
        (fixture.events[1][17] & RUNA_WATCHDOG_STATUS_ARMED) == 0u ||
        runa_read_u32_le(fixture.events[1] + 20u) != 1000u) return 4;

    summary = run_job(&fixture, &mock, &resource, job,
                      make_job(job, RUNA_WATCHDOG_OP_FEED, payload, 2u), &registry);
    if (summary.error != RUNA_OK || mock.feed_calls != 1u || mock.feed_count != 1u) return 5;
    summary = run_job(&fixture, &mock, &resource, job,
                      make_job(job, RUNA_WATCHDOG_OP_DISARM, payload, 2u), &registry);
    if (summary.error != RUNA_OK || mock.disarm_calls != 1u || mock.armed != 0u) return 6;

    resource.permissions = RUNA_PERMISSION_READ;
    summary = run_job(&fixture, &mock, &resource, job,
                      make_job(job, RUNA_WATCHDOG_OP_ARM, payload, 6u), &registry);
    if (summary.error != RUNA_ERR_ACCESS_DENIED || summary.accepted != 0u || mock.arm_calls != 1u) return 7;
    resource.permissions = RUNA_PERMISSION_WRITE;
    summary = run_job(&fixture, &mock, &resource, job,
                      make_job(job, RUNA_WATCHDOG_OP_GET_STATUS, payload, 2u), &registry);
    if (summary.error != RUNA_ERR_ACCESS_DENIED || summary.accepted != 0u || mock.status_calls != 2u) return 8;

    resource.permissions = RUNA_PERMISSION_WRITE;
    runa_write_u32_le(payload + 2u, 99u);
    summary = run_job(&fixture, &mock, &resource, job,
                      make_job(job, RUNA_WATCHDOG_OP_ARM, payload, 6u), &registry);
    if (summary.error != RUNA_ERR_OUT_OF_RANGE || mock.arm_calls != 1u) return 9;
    runa_write_u32_le(payload + 2u, 10001u);
    summary = run_job(&fixture, &mock, &resource, job,
                      make_job(job, RUNA_WATCHDOG_OP_ARM, payload, 6u), &registry);
    if (summary.error != RUNA_ERR_OUT_OF_RANGE || mock.arm_calls != 1u) return 10;

    resource.permissions = RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE;
    configuration.supported_operations = RUNA_WATCHDOG_SUPPORT_STATUS |
                                         RUNA_WATCHDOG_SUPPORT_ARM |
                                         RUNA_WATCHDOG_SUPPORT_FEED;
    summary = run_job(&fixture, &mock, &resource, job,
                      make_job(job, RUNA_WATCHDOG_OP_DISARM, payload, 2u), &registry);
    if (summary.error != RUNA_WATCHDOG_ERR_UNSUPPORTED || mock.disarm_calls != 1u) return 11;
    configuration.supported_operations |= RUNA_WATCHDOG_SUPPORT_DISARM;

    mock.next_status = RUNA_ERR_IO_TIMEOUT;
    summary = run_job(&fixture, &mock, &resource, job,
                      make_job(job, RUNA_WATCHDOG_OP_FEED, payload, 2u), &registry);
    if (summary.error != RUNA_ERR_IO_TIMEOUT || mock.feed_calls != 2u) return 12;

    mock.armed = 1u; mock.timeout_ms = 1000u; mock.last_reset_watchdog = 1u;
    summary = run_job(&fixture, &mock, &resource, job,
                      make_job(job, RUNA_WATCHDOG_OP_GET_STATUS, payload, 2u), &registry);
    if (summary.error != RUNA_OK ||
        (fixture.events[1][17] & RUNA_WATCHDOG_STATUS_LAST_RESET_WATCHDOG) == 0u) return 13;

    mock.arm_calls = 1u;
    summary = run_job(&fixture, &mock, &resource, job, make_malformed_after_arm(job), &registry);
    if (summary.error != RUNA_ERR_INVALID_OPERAND || mock.arm_calls != 1u || summary.accepted != 0u) return 14;

    resource.module_id = 1u;
    summary = run_job(&fixture, &mock, &resource, job,
                      make_job(job, RUNA_WATCHDOG_OP_ARM, payload, 6u), &registry);
    if (summary.error != RUNA_ERR_INVALID_RESOURCE || mock.arm_calls != 1u) return 15;

    puts("Runa.Watchdog validation, authority, persistence and expiry checks passed");
    return 0;
}
