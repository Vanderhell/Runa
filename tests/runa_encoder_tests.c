#include "runa_encoder.h"
#include "runa_capabilities.h"
#include "runa_ir.h"
#include "runa_runtime.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct encoder_fixture {
    uint8_t events[8][RUNA_MAX_EVENT_BYTES];
    size_t sizes[8];
    uint8_t event_count;
    int32_t position;
    int32_t reset_value;
    runa_status_t read_status;
    runa_status_t reset_status;
    runa_status_t read_reset_status;
    uint8_t read_calls;
    uint8_t reset_calls;
    uint8_t read_reset_calls;
    uint8_t order[4];
    uint8_t order_count;
} encoder_fixture_t;

static int capture(void *context, const uint8_t *event, size_t size) {
    encoder_fixture_t *fixture = (encoder_fixture_t *)context;
    if (fixture->event_count >= 8u || size > RUNA_MAX_EVENT_BYTES) return -1;
    memcpy(fixture->events[fixture->event_count], event, size);
    fixture->sizes[fixture->event_count] = size;
    ++fixture->event_count;
    return 0;
}

static uint64_t time_us(void *context) {
    (void)context;
    return 0u;
}

static runa_status_t read_position(void *context, uintptr_t handle, int32_t *position) {
    encoder_fixture_t *fixture = (encoder_fixture_t *)context;
    ++fixture->read_calls;
    if (handle != (uintptr_t)0x1234u || position == NULL) return RUNA_ERR_INVALID_FORMAT;
    if (fixture->order_count < sizeof fixture->order) fixture->order[fixture->order_count++] = 1u;
    if (fixture->read_status != RUNA_OK) return fixture->read_status;
    *position = fixture->position;
    return RUNA_OK;
}

static runa_status_t reset_position(void *context, uintptr_t handle) {
    encoder_fixture_t *fixture = (encoder_fixture_t *)context;
    ++fixture->reset_calls;
    if (handle != (uintptr_t)0x1234u) return RUNA_ERR_INVALID_FORMAT;
    if (fixture->order_count < sizeof fixture->order) fixture->order[fixture->order_count++] = 2u;
    if (fixture->reset_status != RUNA_OK) return fixture->reset_status;
    fixture->position = fixture->reset_value;
    return RUNA_OK;
}

static runa_status_t read_reset_position(void *context, uintptr_t handle, int32_t *position) {
    encoder_fixture_t *fixture = (encoder_fixture_t *)context;
    int32_t previous;
    ++fixture->read_reset_calls;
    if (handle != (uintptr_t)0x1234u || position == NULL) return RUNA_ERR_INVALID_FORMAT;
    if (fixture->order_count < sizeof fixture->order) fixture->order[fixture->order_count++] = 3u;
    if (fixture->read_reset_status != RUNA_OK) return fixture->read_reset_status;
    previous = fixture->position;
    fixture->position = fixture->reset_value;
    *position = previous;
    return RUNA_OK;
}

static size_t make_job(uint8_t *job, uint8_t operation, uint16_t resource_id, uint8_t reg) {
    uint8_t payload_size = operation == RUNA_ENCODER_OP_RESET ? 2u : 3u;
    uint8_t *instruction;
    uint32_t instruction_bytes = (uint32_t)(2u + 3u + payload_size + 3u);
    memset(job, 0, RUNA_HEADER_SIZE + instruction_bytes);
    job[0] = (uint8_t)'J'; job[1] = (uint8_t)'E'; job[2] = (uint8_t)'X'; job[3] = (uint8_t)'E';
    job[4] = RUNA_PROTOCOL_VERSION; job[5] = RUNA_IR_VERSION_V2;
    runa_write_u16_le(job + 6u, RUNA_HEADER_SIZE);
    runa_write_u32_le(job + 8u, 0x10203040u);
    runa_write_u32_le(job + 12u, RUNA_HEADER_SIZE + instruction_bytes);
    runa_write_u32_le(job + 16u, instruction_bytes);
    runa_write_u16_le(job + 20u, 2u);
    runa_write_u32_le(job + 24u, 20u);
    runa_write_u32_le(job + 28u, 1000u);
    runa_write_u16_le(job + 32u, RUNA_MAX_RESULT_BYTES);
    runa_write_u16_le(job + 34u, 8u);
    runa_write_u32_le(job + 36u, 32u);
    instruction = job + RUNA_HEADER_SIZE;
    instruction[0] = RUNA_OP_EXT;
    instruction[1] = (uint8_t)(3u + payload_size);
    runa_write_u16_le(instruction + 2u, RUNA_ENCODER_MODULE_ID);
    instruction[4] = operation;
    runa_write_u16_le(instruction + 5u, resource_id);
    if (operation != RUNA_ENCODER_OP_RESET) instruction[7] = reg;
    instruction += 2u + instruction[1];
    instruction[0] = RUNA_OP_RETURN;
    instruction[1] = 1u;
    instruction[2] = (uint8_t)(operation == RUNA_ENCODER_OP_RESET ? 0u : (1u << reg));
    return RUNA_HEADER_SIZE + instruction_bytes;
}

static size_t make_adversarial_job(uint8_t *job, uint8_t operation, uint16_t resource_id) {
    size_t size = make_job(job, operation, resource_id, 0u);
    size_t invalid_offset = size - 3u;
    memmove(job + invalid_offset + 5u, job + invalid_offset, 3u);
    job[invalid_offset] = RUNA_OP_EXT; job[invalid_offset + 1u] = 3u;
    runa_write_u16_le(job + invalid_offset + 2u, RUNA_ENCODER_MODULE_ID);
    job[invalid_offset + 4u] = 99u;
    runa_write_u32_le(job + 12u, (uint32_t)(size + 5u));
    runa_write_u32_le(job + 16u, (uint32_t)(size + 5u - RUNA_HEADER_SIZE));
    runa_write_u16_le(job + 20u, 3u);
    return size + 5u;
}

static runa_execution_summary_t run_job(uint8_t *job, size_t size,
                                        encoder_fixture_t *fixture,
                                        runa_resource_table_t *resources,
                                        runa_module_registry_t *registry) {
    runa_platform_t platform = { fixture, time_us, NULL };
    runa_event_sink_t sink = { capture, fixture };
    return runa_process(job, size, resources, registry, &platform, &sink);
}

static int expect_error(runa_status_t expected, uint8_t *job, size_t size,
                        encoder_fixture_t *fixture, runa_resource_table_t *resources,
                        runa_module_registry_t *registry) {
    runa_execution_summary_t summary;
    uint8_t read_calls = fixture->read_calls;
    uint8_t reset_calls = fixture->reset_calls;
    uint8_t read_reset_calls = fixture->read_reset_calls;
    fixture->event_count = 0u;
    summary = run_job(job, size, fixture, resources, registry);
    return summary.error == expected && fixture->event_count == 1u &&
           fixture->events[0][0] == RUNA_EVENT_RESULT &&
           fixture->events[0][9] == expected &&
           read_calls == fixture->read_calls && reset_calls == fixture->reset_calls &&
           read_reset_calls == fixture->read_reset_calls;
}

int main(void) {
    encoder_fixture_t fixture = {0};
    runa_encoder_resource_config_t configuration = { 25, 26, RUNA_ENCODER_DECODE_X4, 0u, 7, 0u };
    runa_encoder_hal_t hal = { &fixture, read_position, reset_position, read_reset_position };
    runa_module_t encoder = runa_encoder_module(&hal);
    runa_module_registry_t registry;
    runa_resource_t resource = { 10u, RUNA_ENCODER_MODULE_ID, RUNA_ENCODER_RESOURCE_TYPE, 0u,
                                 RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE,
                                 (uintptr_t)0x1234u, &configuration };
    runa_resource_table_t resources = { &resource, 1u };
    uint8_t job[RUNA_MAX_JOB_BYTES];
    uint8_t capability[64];
    size_t capability_size = 0u;
    runa_capabilities_view_t view;
    runa_execution_summary_t summary;
    int32_t expected_position;

    fixture.reset_value = configuration.reset_value;
    runa_registry_init(&registry);
    if (runa_registry_add(&registry, &encoder) != RUNA_OK ||
        runa_resource_table_validate(&resources, &registry) != RUNA_OK ||
        runa_capabilities_encode(&registry, capability, sizeof capability, &capability_size) != RUNA_OK ||
        runa_capabilities_decode(capability, capability_size, &view) != RUNA_OK ||
        view.module_count != 1u || view.modules[0].module_id != RUNA_ENCODER_MODULE_ID ||
        view.modules[0].payload_size != 5u || view.modules[0].payload[0] != 1u ||
        view.modules[0].payload[1] != 7u || view.modules[0].payload[2] != 32u ||
        view.modules[0].payload[3] != RUNA_ENCODER_DECODE_X4 ||
        view.modules[0].payload[4] != 3u) return 1;

    fixture.position = 0;
    summary = run_job(job, make_job(job, RUNA_ENCODER_OP_READ, resource.id, 0u),
                      &fixture, &resources, &registry);
    if (summary.error != RUNA_OK || !summary.accepted || !summary.result_sent ||
        fixture.event_count != 2u || fixture.events[0][0] != RUNA_EVENT_ACK ||
        fixture.events[1][0] != RUNA_EVENT_RESULT || fixture.events[1][9] != RUNA_OK ||
        runa_read_u16_le(fixture.events[1] + 16u) != 4u ||
        runa_read_u32_le(fixture.events[1] + 20u) != 0u || fixture.read_calls != 1u) return 2;

    expected_position = -123456;
    fixture.position = expected_position;
    fixture.event_count = 0u;
    summary = run_job(job, make_job(job, RUNA_ENCODER_OP_READ, resource.id, 1u),
                      &fixture, &resources, &registry);
    if (summary.error != RUNA_OK || fixture.event_count != 2u ||
        runa_read_u32_le(fixture.events[1] + 20u) != (uint32_t)expected_position) return 3;

    fixture.position = INT32_MAX;
    fixture.event_count = 0u;
    summary = run_job(job, make_job(job, RUNA_ENCODER_OP_READ, resource.id, 0u),
                      &fixture, &resources, &registry);
    if (summary.error != RUNA_OK || runa_read_u32_le(fixture.events[1] + 20u) != 0x7fffffffu)
        return 4;
    fixture.position = INT32_MIN;
    fixture.event_count = 0u;
    summary = run_job(job, make_job(job, RUNA_ENCODER_OP_READ, resource.id, 0u),
                      &fixture, &resources, &registry);
    if (summary.error != RUNA_OK || runa_read_u32_le(fixture.events[1] + 20u) != 0x80000000u)
        return 5;

    fixture.position = 10;
    fixture.order_count = 0u;
    fixture.event_count = 0u;
    summary = run_job(job, make_job(job, RUNA_ENCODER_OP_READ_RESET, resource.id, 0u),
                      &fixture, &resources, &registry);
    if (summary.error != RUNA_OK || fixture.read_reset_calls != 1u || fixture.position != 7 ||
        fixture.event_count != 2u || runa_read_u32_le(fixture.events[1] + 20u) != 10u ||
        fixture.order[fixture.order_count - 1u] != 3u) return 6;

    fixture.position = 12;
    fixture.event_count = 0u;
    summary = run_job(job, make_job(job, RUNA_ENCODER_OP_RESET, resource.id, 0u),
                      &fixture, &resources, &registry);
    if (summary.error != RUNA_OK || fixture.reset_calls != 1u || fixture.position != 7 ||
        fixture.event_count != 2u) return 7;

    fixture.position = 10;
    fixture.event_count = 0u;
    (void)run_job(job, make_job(job, RUNA_ENCODER_OP_READ, resource.id, 0u),
                  &fixture, &resources, &registry);
    fixture.position += 5;
    fixture.event_count = 0u;
    (void)run_job(job, make_job(job, RUNA_ENCODER_OP_READ, resource.id, 0u),
                  &fixture, &resources, &registry);
    if (runa_read_u32_le(fixture.events[1] + 20u) != 15u) return 8;

    fixture.read_status = RUNA_ENCODER_ERR_OVERFLOW;
    fixture.event_count = 0u;
    if (run_job(job, make_job(job, RUNA_ENCODER_OP_READ, resource.id, 0u),
                &fixture, &resources, &registry).error != RUNA_ENCODER_ERR_OVERFLOW ||
        fixture.event_count != 2u) return 9;
    fixture.read_status = RUNA_OK;
    fixture.position = 21;
    fixture.event_count = 0u;
    (void)run_job(job, make_job(job, RUNA_ENCODER_OP_READ, resource.id, 0u),
                  &fixture, &resources, &registry);
    if (runa_read_u32_le(fixture.events[1] + 20u) != 21u) return 10;

    resource.permissions = RUNA_PERMISSION_WRITE;
    if (!expect_error(RUNA_ERR_ACCESS_DENIED,
                      job, make_job(job, RUNA_ENCODER_OP_READ, resource.id, 0u),
                      &fixture, &resources, &registry)) return 11;
    resource.permissions = RUNA_PERMISSION_READ;
    if (!expect_error(RUNA_ERR_ACCESS_DENIED,
                      job, make_job(job, RUNA_ENCODER_OP_RESET, resource.id, 0u),
                      &fixture, &resources, &registry) ||
        !expect_error(RUNA_ERR_ACCESS_DENIED,
                      job, make_job(job, RUNA_ENCODER_OP_READ_RESET, resource.id, 0u),
                      &fixture, &resources, &registry)) return 12;
    resource.permissions = RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE;

    if (!expect_error(RUNA_ERR_INVALID_OPCODE,
                      job, make_job(job, 99u, resource.id, 0u),
                      &fixture, &resources, &registry)) return 13;
    {
        size_t malformed_size = make_job(job, RUNA_ENCODER_OP_RESET, resource.id, 0u);
        job[41] = 4u;
        if (!expect_error(RUNA_ERR_INVALID_OPERAND, job, malformed_size,
                          &fixture, &resources, &registry)) return 14;
    }
    resource.module_id = 9u;
    if (runa_resource_table_validate(&resources, &registry) != RUNA_ERR_INVALID_RESOURCE) return 15;
    resource.module_id = RUNA_ENCODER_MODULE_ID;
    resource.resource_type = 2u;
    if (runa_resource_table_validate(&resources, &registry) != RUNA_ERR_RESOURCE_TYPE) return 16;
    resource.resource_type = RUNA_ENCODER_RESOURCE_TYPE;

    for (unsigned case_index = 0u; case_index < 1000u; ++case_index) {
        uint8_t operation = case_index & 1u ? RUNA_ENCODER_OP_READ_RESET : RUNA_ENCODER_OP_RESET;
        memset(&fixture, 0, sizeof fixture);
        summary = run_job(job, make_adversarial_job(job, operation, resource.id),
                          &fixture, &resources, &registry);
        if (summary.error == RUNA_OK || fixture.reset_calls != 0u ||
            fixture.read_reset_calls != 0u) return 18;
    }

    hal.read_reset = NULL;
    encoder = runa_encoder_module(&hal);
    runa_registry_init(&registry);
    if (runa_registry_add(&registry, &encoder) != RUNA_OK ||
        !expect_error(RUNA_ENCODER_ERR_UNSUPPORTED,
                      job, make_job(job, RUNA_ENCODER_OP_READ_RESET, resource.id, 0u),
                      &fixture, &resources, &registry)) return 17;

    printf("Runa.Encoder signed persistence and permission checks passed\n");
    return 0;
}
