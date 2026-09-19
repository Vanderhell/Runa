#include "runa_can.h"
#include "runa_runtime.h"
#include "runa_ir.h"
#include "runa_capabilities.h"

#include <stdio.h>
#include <string.h>

typedef struct fixture {
    uint8_t events[8][RUNA_MAX_EVENT_BYTES];
    size_t sizes[8];
    uint8_t event_count;
    uint8_t transmit_calls;
    uint8_t receive_calls;
    runa_status_t transmit_status;
    runa_status_t receive_status;
} fixture_t;

static int capture(void *context, const uint8_t *event, size_t size) {
    fixture_t *state = (fixture_t *)context;
    if (state->event_count >= 8u || size > RUNA_MAX_EVENT_BYTES) return -1;
    memcpy(state->events[state->event_count], event, size);
    state->sizes[state->event_count] = size;
    ++state->event_count;
    return 0;
}
static uint64_t time_us(void *context) { (void)context; return 0u; }

static runa_status_t transmit(void *context, uintptr_t handle, const runa_can_frame_t *frame,
                              uint32_t timeout_us) {
    fixture_t *state = (fixture_t *)context;
    ++state->transmit_calls;
    if (handle != (uintptr_t)0x1234u || frame == NULL || frame->id != 0x100u ||
        frame->flags != 0u || frame->length != 2u || frame->data[0] != 0x11u ||
        frame->data[1] != 0x22u || timeout_us != 100000u) return RUNA_CAN_ERR_IO;
    return state->transmit_status;
}

static runa_status_t receive(void *context, uintptr_t handle, runa_can_frame_t *frame,
                             uint32_t timeout_us) {
    fixture_t *state = (fixture_t *)context;
    ++state->receive_calls;
    if (handle != (uintptr_t)0x1234u || frame == NULL || timeout_us != 100000u)
        return RUNA_CAN_ERR_IO;
    if (state->receive_status != RUNA_OK) return state->receive_status;
    frame->id = 0x123u; frame->flags = 0u; frame->length = 8u;
    for (uint8_t index = 0u; index < frame->length; ++index) frame->data[index] = index;
    return RUNA_OK;
}

static size_t make_job(uint8_t *job, uint8_t operation, uint32_t id, uint8_t length) {
    uint8_t *instruction;
    uint8_t *payload;
    uint32_t instruction_bytes;
    size_t payload_size = operation == RUNA_CAN_OP_RECEIVE ? 6u : 12u + length;
    instruction_bytes = (uint32_t)(2u + 3u + payload_size + 3u);
    memset(job, 0, RUNA_HEADER_SIZE + instruction_bytes);
    job[0] = 'J'; job[1] = 'E'; job[2] = 'X'; job[3] = 'E';
    job[4] = RUNA_PROTOCOL_VERSION; job[5] = RUNA_IR_VERSION_V2;
    runa_write_u16_le(job + 6u, RUNA_HEADER_SIZE);
    runa_write_u32_le(job + 8u, 0x10203040u);
    runa_write_u32_le(job + 12u, RUNA_HEADER_SIZE + instruction_bytes);
    runa_write_u32_le(job + 16u, instruction_bytes);
    runa_write_u16_le(job + 20u, 2u); runa_write_u32_le(job + 24u, 20u);
    runa_write_u32_le(job + 28u, 500000u); runa_write_u16_le(job + 32u, RUNA_MAX_RESULT_BYTES);
    runa_write_u16_le(job + 34u, 16u); runa_write_u32_le(job + 36u, 256u);
    instruction = job + RUNA_HEADER_SIZE;
    instruction[0] = RUNA_OP_EXT;
    instruction[1] = (uint8_t)(3u + payload_size);
    runa_write_u16_le(instruction + 2u, RUNA_CAN_MODULE_ID);
    instruction[4] = operation;
    payload = instruction + 5u;
    runa_write_u16_le(payload, 5u);
    if (operation == RUNA_CAN_OP_RECEIVE) {
        runa_write_u32_le(payload + 2u, 100000u);
    } else {
        payload[2] = 0u; runa_write_u32_le(payload + 3u, id); payload[7] = length;
        for (uint8_t index = 0u; index < length; ++index) payload[8u + index] = index == 0u ? 0x11u : 0x22u;
        runa_write_u32_le(payload + 8u + length, 100000u);
    }
    instruction += 2u + instruction[1];
    instruction[0] = RUNA_OP_RETURN; instruction[1] = 1u; instruction[2] = 0u;
    return RUNA_HEADER_SIZE + instruction_bytes;
}

static int invalid_job(fixture_t *state, const runa_resource_table_t *resources,
                       const runa_module_registry_t *registry, const uint8_t *job, size_t size,
                       runa_status_t expected) {
    runa_platform_t platform = { state, time_us, NULL };
    runa_event_sink_t sink = { capture, state };
    runa_execution_summary_t result = runa_process(job, size, resources, registry, &platform, &sink);
    return result.error == expected && state->transmit_calls == 0u && state->receive_calls == 0u &&
           state->event_count == 1u && state->events[0][0] == RUNA_EVENT_RESULT;
}

int main(void) {
    fixture_t state = {0};
    runa_can_resource_config_t configuration = { 0u, RUNA_CAN_FILTER_STANDARD, 0u, 0u,
        500000u, 0x120u, 0x7f0u, 1000000u, 0u };
    runa_can_hal_t hal = { &state, transmit, receive };
    runa_module_t can = runa_can_module(&hal);
    runa_module_registry_t registry;
    runa_resource_t resource = { 5u, RUNA_CAN_MODULE_ID, RUNA_CAN_RESOURCE_TYPE, 0u,
        RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE, (uintptr_t)0x1234u, &configuration };
    runa_resource_table_t resources = { &resource, 1u };
    runa_platform_t platform = { &state, time_us, NULL };
    runa_event_sink_t sink = { capture, &state };
    runa_execution_summary_t result;
    uint8_t job[RUNA_MAX_JOB_BYTES];
    uint8_t capability[64]; size_t capability_size; runa_capabilities_view_t view;

    runa_registry_init(&registry);
    if (runa_registry_add(&registry, &can) != RUNA_OK ||
        runa_capabilities_encode(&registry, capability, sizeof capability, &capability_size) != RUNA_OK ||
        runa_capabilities_decode(capability, capability_size, &view) != RUNA_OK ||
        view.module_count != 1u || view.modules[0].module_id != RUNA_CAN_MODULE_ID ||
        view.modules[0].payload_size != 12u) return 1;

    result = runa_process(job, make_job(job, RUNA_CAN_OP_TRANSMIT, 0x100u, 2u),
                           &resources, &registry, &platform, &sink);
    if (result.error != RUNA_OK || state.transmit_calls != 1u || state.receive_calls != 0u ||
        state.event_count != 2u || state.events[0][0] != RUNA_EVENT_ACK ||
        state.events[1][0] != RUNA_EVENT_RESULT) return 2;

    memset(&state, 0, sizeof state);
    result = runa_process(job, make_job(job, RUNA_CAN_OP_RECEIVE, 0u, 0u),
                           &resources, &registry, &platform, &sink);
    if (result.error != RUNA_OK || state.transmit_calls != 0u || state.receive_calls != 1u ||
        state.event_count != 3u || state.events[1][0] != RUNA_EVENT_MODULE_DATA ||
        state.sizes[1] != 30u || state.events[1][16] != 0u || state.events[1][17] != 0x23u ||
        state.events[1][21] != 8u || state.events[2][0] != RUNA_EVENT_RESULT) return 3;

    memset(&state, 0, sizeof state);
    result = runa_process(job, make_job(job, RUNA_CAN_OP_REQUEST_RESPONSE, 0x100u, 2u),
                           &resources, &registry, &platform, &sink);
    if (result.error != RUNA_OK || state.transmit_calls != 1u || state.receive_calls != 1u ||
        state.event_count != 3u || state.events[1][0] != RUNA_EVENT_MODULE_DATA) return 4;

    memset(&state, 0, sizeof state);
    (void)make_job(job, RUNA_CAN_OP_TRANSMIT, 0x800u, 2u);
    if (!invalid_job(&state, &resources, &registry, job, RUNA_HEADER_SIZE + 22u, RUNA_ERR_INVALID_OPERAND)) return 5;
    memset(&state, 0, sizeof state);
    (void)make_job(job, RUNA_CAN_OP_TRANSMIT, 0x100u, 9u);
    if (!invalid_job(&state, &resources, &registry, job, RUNA_HEADER_SIZE + 29u, RUNA_ERR_INVALID_OPERAND)) return 6;
    memset(&state, 0, sizeof state); state.receive_status = RUNA_ERR_IO_TIMEOUT;
    result = runa_process(job, make_job(job, RUNA_CAN_OP_RECEIVE, 0u, 0u),
                           &resources, &registry, &platform, &sink);
    if (result.error != RUNA_ERR_IO_TIMEOUT || state.receive_calls != 1u || state.event_count != 2u ||
        state.events[0][0] != RUNA_EVENT_ACK || state.events[1][0] != RUNA_EVENT_RESULT) return 7;
    puts("Runa.CAN bounded frame checks passed");
    return 0;
}
