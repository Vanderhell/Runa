#include "runa_uart.h"
#include "runa_runtime.h"
#include "runa_ir.h"
#include "runa_capabilities.h"

#include <stdio.h>
#include <string.h>

typedef struct fixture {
    uint8_t events[8][RUNA_MAX_EVENT_BYTES];
    size_t sizes[8];
    uint8_t event_count;
    uint8_t calls;
    runa_status_t hal_status;
    size_t returned_rx;
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

static runa_status_t transfer(void *context, uintptr_t handle,
                              const runa_uart_resource_config_t *configuration,
                              const uint8_t *transmit, size_t transmit_size,
                              uint8_t *receive, size_t receive_capacity, size_t *receive_size,
                              uint8_t policy, uint32_t timeout_us) {
    fixture_t *state = (fixture_t *)context;
    size_t index;
    ++state->calls;
    if (handle != (uintptr_t)0x4321u || configuration == NULL || configuration->controller != 1u ||
        timeout_us != 100000u || policy > RUNA_UART_RX_UP_TO_LENGTH ||
        (transmit_size != 0u && (transmit == NULL || transmit[0] != 0x55u)) ||
        (receive_capacity != 0u && receive == NULL) || receive_size == NULL)
        return RUNA_UART_ERR_IO;
    if (state->hal_status != RUNA_OK) return state->hal_status;
    *receive_size = state->returned_rx == 0u ? receive_capacity : state->returned_rx;
    for (index = 0u; index < *receive_size; ++index) receive[index] = (uint8_t)(0x80u + index);
    return RUNA_OK;
}

static size_t make_job(uint8_t *job, uint8_t tx_size, uint8_t rx_size, uint8_t policy,
                       uint32_t timeout_us, uint8_t operation, uint8_t operand_size) {
    uint8_t *instruction;
    uint32_t instruction_bytes = (uint32_t)(17u + tx_size);
    memset(job, 0, RUNA_HEADER_SIZE + instruction_bytes);
    job[0] = 'J'; job[1] = 'E'; job[2] = 'X'; job[3] = 'E';
    job[4] = RUNA_PROTOCOL_VERSION; job[5] = RUNA_IR_VERSION_V2;
    runa_write_u16_le(job + 6u, RUNA_HEADER_SIZE);
    runa_write_u32_le(job + 8u, 0x55667788u);
    runa_write_u32_le(job + 12u, RUNA_HEADER_SIZE + instruction_bytes);
    runa_write_u32_le(job + 16u, instruction_bytes);
    runa_write_u16_le(job + 20u, 2u); runa_write_u32_le(job + 24u, 20u);
    runa_write_u32_le(job + 28u, 500000u); runa_write_u16_le(job + 32u, RUNA_MAX_RESULT_BYTES);
    runa_write_u16_le(job + 34u, 16u); runa_write_u32_le(job + 36u, 256u);
    instruction = job + RUNA_HEADER_SIZE;
    instruction[0] = RUNA_OP_EXT; instruction[1] = operand_size;
    runa_write_u16_le(instruction + 2u, RUNA_UART_MODULE_ID); instruction[4] = operation;
    runa_write_u16_le(instruction + 5u, 8u); instruction[7] = tx_size; instruction[8] = rx_size;
    instruction[9] = policy; runa_write_u32_le(instruction + 10u, timeout_us);
    if (tx_size != 0u) instruction[14] = 0x55u;
    instruction += 14u + tx_size;
    instruction[0] = RUNA_OP_RETURN; instruction[1] = 1u; instruction[2] = 0u;
    return RUNA_HEADER_SIZE + instruction_bytes;
}

static size_t make_adversarial_job(uint8_t *job) {
    size_t size = make_job(job, 1u, 1u, RUNA_UART_RX_FIXED_LENGTH, 100000u,
                            RUNA_UART_OP_TRANSFER, 13u);
    size_t invalid_offset = size - 3u;
    memmove(job + invalid_offset + 5u, job + invalid_offset, 3u);
    job[invalid_offset] = RUNA_OP_EXT;
    job[invalid_offset + 1u] = 3u;
    runa_write_u16_le(job + invalid_offset + 2u, RUNA_UART_MODULE_ID);
    job[invalid_offset + 4u] = 99u;
    runa_write_u32_le(job + 12u, (uint32_t)(size + 5u));
    runa_write_u32_le(job + 16u, (uint32_t)(size + 5u - RUNA_HEADER_SIZE));
    runa_write_u16_le(job + 20u, 3u);
    return size + 5u;
}

static int invalid_job(fixture_t *state, const runa_resource_table_t *resources,
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
    runa_uart_resource_config_t configuration = { 1u, 8u, 1u, RUNA_UART_PARITY_NONE,
        RUNA_UART_FLOW_NONE, 0u, 17, 18, 115200u, 64u, 64u, 1000000u, 0u };
    runa_uart_hal_t hal = { &state, transfer };
    runa_module_t uart = runa_uart_module(&hal);
    runa_module_registry_t registry;
    runa_resource_t resource = { 8u, RUNA_UART_MODULE_ID, RUNA_UART_RESOURCE_TYPE, 0u,
        RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE, (uintptr_t)0x4321u, &configuration };
    runa_resource_table_t resources = { &resource, 1u };
    runa_platform_t platform = { &state, time_us, NULL };
    runa_event_sink_t sink = { capture, &state };
    runa_execution_summary_t result;
    uint8_t job[RUNA_MAX_JOB_BYTES];
    uint8_t capabilities[64];
    size_t capability_size;
    runa_capabilities_view_t view;

    runa_registry_init(&registry);
    if (runa_registry_add(&registry, &uart) != RUNA_OK) { (void)printf("registry failure\\n"); return 1; }
    if (runa_capabilities_encode(&registry, capabilities, sizeof capabilities, &capability_size) != RUNA_OK) { (void)printf("cap encode failure\\n"); return 1; }
    if (runa_capabilities_decode(capabilities, capability_size, &view) != RUNA_OK) { (void)printf("cap decode failure\\n"); return 1; }
    if (
        view.module_count != 1u || view.modules[0].module_id != RUNA_UART_MODULE_ID ||
        view.modules[0].payload_size != 12u) { (void)printf("capability failure count=%u payload=%u\\n", view.module_count, view.module_count == 0u ? 0u : view.modules[0].payload_size); return 1; }

    if (runa_resource_table_validate(&resources, &registry) != RUNA_OK) return 8;
    result = runa_process(job, make_job(job, 1u, 51u, RUNA_UART_RX_FIXED_LENGTH, 100000u,
                                        RUNA_UART_OP_TRANSFER, 13u), &resources, &registry,
                           &platform, &sink);
    if (result.error != RUNA_OK || state.calls != 1u || state.event_count != 4u ||
        state.events[0][0] != RUNA_EVENT_ACK || state.events[1][0] != RUNA_EVENT_MODULE_DATA ||
        state.events[2][0] != RUNA_EVENT_MODULE_DATA || state.events[3][0] != RUNA_EVENT_RESULT ||
        state.sizes[1] != 64u || state.sizes[2] != 19u || state.events[1][16] != 0x80u ||
        state.events[2][16] != 0xb0u) { (void)printf("first failure result=%u calls=%u events=%u\\n", result.error, state.calls, state.event_count); return 2; }

    memset(&state, 0, sizeof state);
    state.returned_rx = 2u;
    result = runa_process(job, make_job(job, 0u, 10u, RUNA_UART_RX_UP_TO_LENGTH, 100000u,
                                        RUNA_UART_OP_TRANSFER, 12u), &resources, &registry,
                           &platform, &sink);
    if (result.error != RUNA_OK || state.calls != 1u || state.event_count != 3u ||
        state.events[0][0] != RUNA_EVENT_ACK || state.events[1][0] != RUNA_EVENT_MODULE_DATA ||
        state.events[2][0] != RUNA_EVENT_RESULT || state.sizes[1] != 18u) return 3;

    memset(&state, 0, sizeof state);
    (void)make_job(job, 1u, 1u, RUNA_UART_RX_FIXED_LENGTH, 100000u, 99u, 13u);
    if (!invalid_job(&state, &resources, &registry, job, RUNA_HEADER_SIZE + 18u, RUNA_ERR_INVALID_OPCODE)) return 4;
    memset(&state, 0, sizeof state);
    (void)make_job(job, 1u, 1u, RUNA_UART_RX_FIXED_LENGTH, 100000u, RUNA_UART_OP_TRANSFER, 12u);
    if (!invalid_job(&state, &resources, &registry, job, RUNA_HEADER_SIZE + 18u, RUNA_ERR_INVALID_OPERAND)) return 5;
    memset(&state, 0, sizeof state);
    configuration.maximum_tx_bytes = 0u;
    if (runa_resource_table_validate(&resources, &registry) != RUNA_ERR_INVALID_RESOURCE) return 6;
    configuration.maximum_tx_bytes = 64u;
    memset(&state, 0, sizeof state);
    state.hal_status = RUNA_ERR_IO_TIMEOUT;
    result = runa_process(job, make_job(job, 1u, 1u, RUNA_UART_RX_FIXED_LENGTH, 100000u,
                                        RUNA_UART_OP_TRANSFER, 13u), &resources, &registry,
                           &platform, &sink);
    if (result.error != RUNA_ERR_IO_TIMEOUT || state.calls != 1u || state.event_count != 2u ||
        state.events[0][0] != RUNA_EVENT_ACK || state.events[1][0] != RUNA_EVENT_RESULT) return 7;
    configuration.maximum_tx_bytes = 64u;
    configuration.maximum_rx_bytes = 64u;
    resource.permissions = RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE;
    for (unsigned case_index = 0u; case_index < 1000u; ++case_index) {
        memset(&state, 0, sizeof state);
        result = runa_process(job, make_adversarial_job(job), &resources, &registry, &platform, &sink);
        if (result.error == RUNA_OK || state.calls != 0u) return 8;
    }
    puts("Runa.UART bounded transfer checks passed");
    return 0;
}
