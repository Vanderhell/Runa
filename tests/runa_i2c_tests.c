#include "runa_i2c.h"
#include "runa_runtime.h"
#include "runa_ir.h"
#include "runa_capabilities.h"

#include <stdio.h>
#include <string.h>

typedef struct fixture {
    uint8_t events[8][RUNA_MAX_EVENT_BYTES];
    size_t event_sizes[8];
    uint8_t event_count;
    uint8_t calls;
    runa_status_t hal_status;
} fixture_t;

static int capture(void *context, const uint8_t *event, size_t size) {
    fixture_t *state = (fixture_t *)context;
    if (state->event_count >= 8u || size > RUNA_MAX_EVENT_BYTES) return -1;
    memcpy(state->events[state->event_count], event, size);
    state->event_sizes[state->event_count] = size;
    ++state->event_count;
    return 0;
}

static uint64_t time_us(void *context) { (void)context; return 0u; }

static runa_status_t transfer(void *context, uintptr_t handle,
                              const runa_i2c_resource_config_t *configuration,
                              const uint8_t *transmit, size_t transmit_size,
                              uint8_t *receive, size_t receive_size,
                              uint16_t timeout_ms) {
    fixture_t *state = (fixture_t *)context;
    size_t index;
    ++state->calls;
    if (handle != (uintptr_t)0x1234u || configuration == NULL || configuration->address != 0x48u ||
        configuration->bus != 0u || timeout_ms != 100u || transmit_size > 3u ||
        (transmit_size != 0u && transmit == NULL) || (receive_size != 0u && receive == NULL))
        return RUNA_I2C_ERR_BUS;
    if (state->hal_status != RUNA_OK) return state->hal_status;
    for (index = 0u; index < receive_size; ++index) receive[index] = (uint8_t)(0xa0u + index);
    return RUNA_OK;
}

static size_t make_job(uint8_t *job, uint8_t tx_size, uint8_t rx_size, uint16_t timeout_ms,
                       uint8_t operation, uint8_t declared_operand_size) {
    uint8_t *instruction;
    uint32_t instruction_bytes = (uint32_t)(14u + tx_size);
    memset(job, 0, RUNA_HEADER_SIZE + instruction_bytes);
    job[0] = 'J'; job[1] = 'E'; job[2] = 'X'; job[3] = 'E';
    job[4] = RUNA_PROTOCOL_VERSION; job[5] = RUNA_IR_VERSION_V2;
    runa_write_u16_le(job + 6u, RUNA_HEADER_SIZE);
    runa_write_u32_le(job + 8u, 0x10203040u);
    runa_write_u32_le(job + 12u, RUNA_HEADER_SIZE + instruction_bytes);
    runa_write_u32_le(job + 16u, instruction_bytes);
    runa_write_u16_le(job + 20u, 2u);
    runa_write_u32_le(job + 24u, 20u);
    runa_write_u32_le(job + 28u, 500000u);
    runa_write_u16_le(job + 32u, RUNA_MAX_RESULT_BYTES);
    runa_write_u16_le(job + 34u, 16u);
    runa_write_u32_le(job + 36u, 256u);
    instruction = job + RUNA_HEADER_SIZE;
    instruction[0] = RUNA_OP_EXT;
    instruction[1] = declared_operand_size;
    runa_write_u16_le(instruction + 2u, RUNA_I2C_MODULE_ID);
    instruction[4] = operation;
    runa_write_u16_le(instruction + 5u, 5u);
    instruction[7] = tx_size;
    instruction[8] = rx_size;
    runa_write_u16_le(instruction + 9u, timeout_ms);
    if (tx_size != 0u) { instruction[11] = 0x10u; instruction[12] = 0x20u; instruction[13] = 0x30u; }
    instruction += 11u + tx_size;
    instruction[0] = RUNA_OP_RETURN; instruction[1] = 1u; instruction[2] = 0u;
    return RUNA_HEADER_SIZE + instruction_bytes;
}

static size_t make_adversarial_job(uint8_t *job) {
    size_t size = make_job(job, 3u, 1u, 100u, RUNA_I2C_OP_TRANSFER, 12u);
    size_t invalid_offset = size - 3u;
    memmove(job + invalid_offset + 5u, job + invalid_offset, 3u);
    job[invalid_offset] = RUNA_OP_EXT;
    job[invalid_offset + 1u] = 3u;
    runa_write_u16_le(job + invalid_offset + 2u, RUNA_I2C_MODULE_ID);
    job[invalid_offset + 4u] = 99u;
    runa_write_u32_le(job + 12u, (uint32_t)(size + 5u));
    runa_write_u32_le(job + 16u, (uint32_t)(size + 5u - RUNA_HEADER_SIZE));
    runa_write_u16_le(job + 20u, 3u);
    return size + 5u;
}

static int expect_error(fixture_t *state, const runa_resource_table_t *resources,
                        const runa_module_registry_t *registry, const uint8_t *job, size_t size,
                        runa_status_t expected) {
    runa_platform_t platform = { state, time_us, NULL };
    runa_event_sink_t sink = { capture, state };
    runa_execution_summary_t result = runa_process(job, size, resources, registry, &platform, &sink);
    return result.error == expected && state->calls == 0u && state->event_count == 1u &&
           state->events[0][0] == RUNA_EVENT_RESULT;
}

int main(void) {
    fixture_t state = {0};
    runa_i2c_resource_config_t configuration = { 0u, 0x48u, 64u, 64u, 400000u, 1000u, 0u };
    runa_i2c_hal_t hal = { &state, transfer };
    runa_module_t i2c = runa_i2c_module(&hal);
    runa_module_registry_t registry;
    runa_resource_t resource = { 5u, RUNA_I2C_MODULE_ID, RUNA_I2C_RESOURCE_TYPE, 0u,
                                 RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE,
                                 (uintptr_t)0x1234u, &configuration };
    runa_resource_table_t resources = { &resource, 1u };
    runa_platform_t platform = { &state, time_us, NULL };
    runa_event_sink_t sink = { capture, &state };
    runa_execution_summary_t result;
    uint8_t job[RUNA_MAX_JOB_BYTES];
    uint8_t capability_bytes[128];
    size_t capability_size;
    runa_capabilities_view_t capability_view;

    runa_registry_init(&registry);
    if (runa_registry_add(&registry, &i2c) != RUNA_OK ||
        runa_capabilities_encode(&registry, capability_bytes, sizeof capability_bytes, &capability_size) != RUNA_OK ||
        runa_capabilities_decode(capability_bytes, capability_size, &capability_view) != RUNA_OK ||
        capability_view.module_count != 1u || capability_view.modules[0].module_id != RUNA_I2C_MODULE_ID ||
        capability_view.modules[0].payload_size != 8u ||
        capability_view.modules[0].payload[0] != RUNA_I2C_OP_TRANSFER ||
        capability_view.modules[0].payload[1] != RUNA_I2C_MAX_TX_BYTES ||
        capability_view.modules[0].payload[2] != RUNA_I2C_MAX_RX_BYTES) return 1;

    result = runa_process(job, make_job(job, 3u, 51u, 100u, RUNA_I2C_OP_TRANSFER, 12u),
                          &resources, &registry, &platform, &sink);
    if (result.error != RUNA_OK || state.calls != 1u || state.event_count != 4u ||
        state.events[0][0] != RUNA_EVENT_ACK || state.events[1][0] != RUNA_EVENT_MODULE_DATA ||
        state.events[2][0] != RUNA_EVENT_MODULE_DATA || state.events[3][0] != RUNA_EVENT_RESULT ||
        state.event_sizes[1] != 64u || state.event_sizes[2] != 19u ||
        runa_read_u16_le(state.events[1] + 8u) != RUNA_I2C_MODULE_ID ||
        state.events[1][16] != 0xa0u || state.events[2][16] != 0xd0u) return 2;

    memset(&state, 0, sizeof state);
    (void)make_job(job, 0u, 0u, 100u, RUNA_I2C_OP_TRANSFER, 6u);
    if (!expect_error(&state, &resources, &registry, job, RUNA_HEADER_SIZE + 14u, RUNA_ERR_INVALID_OPERAND)) return 3;
    memset(&state, 0, sizeof state);
    configuration.maximum_tx_bytes = 2u;
    (void)make_job(job, 3u, 0u, 100u, RUNA_I2C_OP_TRANSFER, 12u);
    if (!expect_error(&state, &resources, &registry, job, RUNA_HEADER_SIZE + 17u, RUNA_ERR_OUT_OF_RANGE)) return 31;
    configuration.maximum_tx_bytes = 64u;
    memset(&state, 0, sizeof state);
    (void)make_job(job, 3u, 1u, 100u, 99u, 12u);
    if (!expect_error(&state, &resources, &registry, job, RUNA_HEADER_SIZE + 17u, RUNA_ERR_INVALID_OPCODE)) return 4;
    memset(&state, 0, sizeof state);
    (void)make_job(job, 3u, 1u, 100u, RUNA_I2C_OP_TRANSFER, 11u);
    if (!expect_error(&state, &resources, &registry, job, RUNA_HEADER_SIZE + 17u, RUNA_ERR_INVALID_OPERAND)) return 5;
    memset(&state, 0, sizeof state);
    resource.permissions = RUNA_PERMISSION_WRITE;
    (void)make_job(job, 0u, 1u, 100u, RUNA_I2C_OP_TRANSFER, 9u);
    if (!expect_error(&state, &resources, &registry, job, RUNA_HEADER_SIZE + 14u, RUNA_ERR_ACCESS_DENIED)) return 6;
    resource.permissions = RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE;
    configuration.address = 0x80u;
    if (runa_resource_table_validate(&resources, &registry) != RUNA_ERR_INVALID_RESOURCE) return 7;

    configuration.address = 0x48u;
    resource.permissions = RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE;
    for (unsigned case_index = 0u; case_index < 1000u; ++case_index) {
        memset(&state, 0, sizeof state);
        result = runa_process(job, make_adversarial_job(job), &resources, &registry, &platform, &sink);
        if (result.error == RUNA_OK || state.calls != 0u) return 8;
    }
    resource.permissions = RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE;
    configuration.address = 0x48u;
    configuration.maximum_tx_bytes = 64u;
    configuration.maximum_rx_bytes = 64u;
    for (unsigned case_index = 0u; case_index < 10000u; ++case_index) {
        unsigned variant = case_index % 8u;
        memset(&state, 0, sizeof state);
        if (variant == 0u) {
            result = runa_process(job, make_job(job, 3u, 1u, 100u, RUNA_I2C_OP_TRANSFER, 12u),
                                   &resources, &registry, &platform, &sink);
            if (result.error != RUNA_OK || state.calls != 1u) return 9;
        } else {
            uint8_t tx = variant == 1u || variant >= 6u ? 65u : 3u;
            uint8_t rx = variant == 2u ? 65u : 1u;
            uint16_t timeout = variant == 3u ? 0u : 100u;
            uint8_t operation = variant == 4u ? 99u : RUNA_I2C_OP_TRANSFER;
            uint8_t operand_size = variant == 5u ? 11u : 12u;
            result = runa_process(job, make_job(job, tx, rx, timeout, operation, operand_size),
                                   &resources, &registry, &platform, &sink);
            if (result.error == RUNA_OK || state.calls != 0u) return 10;
        }
    }

    puts("Runa.I2C bounded transfer checks passed");
    return 0;
}
