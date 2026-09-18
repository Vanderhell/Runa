#include "runa_gpio.h"
#include "runa_runtime.h"
#include "runa_ir.h"
#include "runa_capabilities.h"

#include <stdio.h>
#include <string.h>

typedef struct fixture {
    uint8_t events[8][RUNA_MAX_EVENT_BYTES];
    size_t event_sizes[8];
    uint8_t event_count;
    uint32_t output;
    uint32_t writes;
    uint32_t fail_write;
    uint32_t begins;
    uint32_t ends;
    uint32_t end_status;
    uint8_t fail_end;
    uint8_t emit_data;
    uint64_t now_us;
} fixture_t;

static runa_module_execute_fn gpio_execute_inner;

static int capture(void *context, const uint8_t *event, size_t size) {
    fixture_t *state = (fixture_t *)context;
    if (state->event_count >= 8u || size > RUNA_MAX_EVENT_BYTES) return -1;
    memcpy(state->events[state->event_count], event, size);
    state->event_sizes[state->event_count] = size;
    ++state->event_count;
    return 0;
}

static runa_status_t gpio_write(void *context, uintptr_t handle, uint32_t value) {
    fixture_t *state = (fixture_t *)context;
    if (handle != 17u) return RUNA_GPIO_ERR_IO;
    ++state->writes;
    if (state->writes == state->fail_write) return RUNA_GPIO_ERR_IO;
    state->output = value;
    return RUNA_OK;
}

static runa_status_t gpio_execute_with_data(void *context, runa_module_job_t *job,
                                            const runa_module_instruction_t *instruction,
                                            uint32_t *detail) {
    static const uint8_t response[] = { 0xa5u, 0x00u };
    runa_gpio_hal_t *hal = (runa_gpio_hal_t *)context;
    fixture_t *state = (fixture_t *)hal->context;
    if (state->emit_data != 0u) {
        runa_status_t status = job->emit_data(job->emit_context, RUNA_GPIO_MODULE_ID,
                                              instruction->instruction_index, 0u,
                                              response, sizeof response);
        if (status != RUNA_OK) return status;
    }
    return gpio_execute_inner(context, job, instruction, detail);
}

static uint64_t time_us(void *context) { return ((fixture_t *)context)->now_us; }

static runa_status_t delay_ms(void *context, uint32_t milliseconds) {
    ((fixture_t *)context)->now_us += (uint64_t)milliseconds * 1000u;
    return RUNA_OK;
}

static runa_status_t module_begin(void *context, const runa_module_job_t *job) {
    runa_gpio_hal_t *hal = (runa_gpio_hal_t *)context;
    fixture_t *state = (fixture_t *)hal->context;
    (void)job;
    ++state->begins;
    return RUNA_OK;
}

static runa_status_t module_end(void *context, const runa_module_job_t *job,
                                runa_status_t execution_status) {
    runa_gpio_hal_t *hal = (runa_gpio_hal_t *)context;
    fixture_t *state = (fixture_t *)hal->context;
    (void)job;
    ++state->ends;
    state->end_status = execution_status;
    return state->fail_end != 0u ? RUNA_ERR_INTERNAL : RUNA_OK;
}

static size_t make_job(uint8_t *output, const uint8_t *instructions, uint16_t count,
                       uint8_t version) {
    size_t instruction_size = 0u;
    uint16_t index;
    for (index = 0u; index < count; ++index) {
        instruction_size += (size_t)instructions[instruction_size + 1u] + 2u;
    }
    memset(output, 0, RUNA_HEADER_SIZE + instruction_size);
    output[0] = (uint8_t)'J'; output[1] = (uint8_t)'E'; output[2] = (uint8_t)'X'; output[3] = (uint8_t)'E';
    output[4] = RUNA_PROTOCOL_VERSION; output[5] = version;
    runa_write_u16_le(output + 6u, RUNA_HEADER_SIZE);
    runa_write_u32_le(output + 8u, 0x10203040u);
    runa_write_u32_le(output + 12u, (uint32_t)(RUNA_HEADER_SIZE + instruction_size));
    runa_write_u32_le(output + 16u, (uint32_t)instruction_size);
    runa_write_u16_le(output + 20u, count);
    runa_write_u32_le(output + 24u, RUNA_MAX_STEPS);
    runa_write_u32_le(output + 28u, RUNA_MAX_RUNTIME_US);
    runa_write_u16_le(output + 32u, RUNA_MAX_RESULT_BYTES);
    runa_write_u16_le(output + 34u, RUNA_MAX_EMITS);
    runa_write_u32_le(output + 36u, RUNA_MAX_EMIT_BYTES);
    memcpy(output + RUNA_HEADER_SIZE, instructions, instruction_size);
    return RUNA_HEADER_SIZE + instruction_size;
}

int main(void) {
    static const uint8_t valid_instructions[] = {
        RUNA_OP_LOAD_CONST, 5u, 0u, 1u, 0u, 0u, 0u,
        RUNA_OP_EXT, 6u, 1u, 0u, RUNA_GPIO_OP_WRITE, 9u, 0u, 0u,
        RUNA_OP_RETURN, 1u, 1u
    };
    static const uint8_t golden_job[] = {
        0x4au,0x45u,0x58u,0x45u,0x01u,0x02u,0x28u,0x00u,
        0x40u,0x30u,0x20u,0x10u,0x3au,0x00u,0x00u,0x00u,
        0x12u,0x00u,0x00u,0x00u,0x03u,0x00u,0x00u,0x00u,
        0x10u,0x27u,0x00u,0x00u,0x40u,0x4bu,0x4cu,0x00u,
        0x20u,0x00u,0x80u,0x00u,0x00u,0x02u,0x00u,0x00u,
        0x01u,0x05u,0x00u,0x01u,0x00u,0x00u,0x00u,
        0xe0u,0x06u,0x01u,0x00u,0x02u,0x09u,0x00u,0x00u,
        0x51u,0x01u,0x01u
    };
    static const uint8_t golden_capabilities[] = {
        2u,1u,2u,32u,44u,0u,1u,8u,0u,8u,0u,0u,0u,1u,32u,0u,
        128u,0u,0u,0u,16u,39u,0u,0u,64u,75u,76u,0u,0u,2u,0u,0u,
        12u,0u,1u,0u,1u,0u,1u,1u,2u,0u,1u,2u
    };
    uint8_t job[RUNA_MAX_JOB_BYTES];
    size_t job_size;
    fixture_t state = {0};
    runa_gpio_hal_t gpio_hal = { &state, NULL, gpio_write };
    runa_module_t gpio = runa_gpio_module(&gpio_hal);
    runa_module_registry_t registry;
    runa_resource_t resource = { 9u, RUNA_GPIO_MODULE_ID, RUNA_GPIO_RESOURCE_TYPE, 0u,
                                 RUNA_PERMISSION_WRITE, 17u, NULL };
    runa_resource_table_t resources = { &resource, 1u };
    runa_platform_t platform = { &state, time_us, delay_ms };
    runa_event_sink_t sink = { capture, &state };
    runa_execution_summary_t result;
    uint8_t capability_bytes[RUNA_MAX_CAPABILITY_BYTES];
    size_t capability_size = 0u;
    runa_capabilities_view_t capability_view;
    gpio.begin = module_begin;
    gpio.end = module_end;
    gpio_execute_inner = gpio.execute;
    gpio.execute = gpio_execute_with_data;

    runa_registry_init(&registry);
    if (runa_registry_add(&registry, &gpio) != RUNA_OK) return 1;
    {
        runa_resource_t wrong_type = { 19u, RUNA_GPIO_MODULE_ID, 99u, 0u,
                                       RUNA_PERMISSION_WRITE, 17u, NULL };
        runa_resource_table_t wrong_table = { &wrong_type, 1u };
        if (runa_resource_table_validate(&wrong_table, &registry) != RUNA_ERR_RESOURCE_TYPE) return 19;
    }
    if (runa_capabilities_encode(&registry, capability_bytes, sizeof capability_bytes,
                                &capability_size) != RUNA_OK) return 12;
    if (capability_size != sizeof golden_capabilities ||
        memcmp(capability_bytes, golden_capabilities, sizeof golden_capabilities) != 0) return 15;
    if (runa_capabilities_decode(capability_bytes, capability_size, &capability_view) != RUNA_OK ||
        capability_view.module_count != 1u || capability_view.modules[0].module_id != RUNA_GPIO_MODULE_ID ||
        capability_view.modules[0].payload_size != 2u || capability_view.modules[0].payload[0] != RUNA_GPIO_OP_READ ||
        capability_view.modules[0].payload[1] != RUNA_GPIO_OP_WRITE ||
        capability_view.max_job_bytes != RUNA_MAX_JOB_BYTES || capability_view.ir_version != RUNA_IR_VERSION_V2)
        return 13;
    {
        uint8_t extended_capabilities[RUNA_MAX_CAPABILITY_BYTES];
        memcpy(extended_capabilities, capability_bytes, RUNA_CAPABILITIES_HEADER_SIZE);
        extended_capabilities[6] = 2u;
        runa_write_u16_le(extended_capabilities + 4u, (uint16_t)(capability_size + 4u));
        extended_capabilities[32] = 4u;
        extended_capabilities[33] = 0u;
        extended_capabilities[34] = 99u;
        extended_capabilities[35] = 0u;
        memcpy(extended_capabilities + 36u, capability_bytes + 32u, capability_size - 32u);
        if (runa_capabilities_decode(extended_capabilities, capability_size + 4u,
                                     &capability_view) != RUNA_OK ||
            capability_view.module_record_count != 2u || capability_view.module_count != 1u)
            return 16;
    }
    capability_bytes[4] = (uint8_t)(capability_bytes[4] + 1u);
    if (runa_capabilities_decode(capability_bytes, capability_size, &capability_view) != RUNA_ERR_INVALID_FORMAT)
        return 14;
    capability_bytes[4] = (uint8_t)(capability_bytes[4] - 1u);
    job_size = make_job(job, valid_instructions, 3u, RUNA_IR_VERSION_V2);
    if (job_size != sizeof golden_job || memcmp(job, golden_job, sizeof golden_job) != 0) return 11;
    result = runa_process(job, job_size, &resources, &registry, &platform, &sink);
    if (result.error != RUNA_OK || result.accepted != 1u || result.result_sent != 1u) return 2;
    if (state.event_count != 2u || state.events[0][0] != RUNA_EVENT_ACK ||
        state.events[1][0] != RUNA_EVENT_RESULT || state.output != 1u || state.writes != 1u ||
        state.begins != 1u || state.ends != 1u || state.end_status != RUNA_OK) return 3;

    memset(&state, 0, sizeof state);
    state.emit_data = 1u;
    job_size = make_job(job, valid_instructions, 3u, RUNA_IR_VERSION_V2);
    result = runa_process(job, job_size, &resources, &registry, &platform, &sink);
    if (result.error != RUNA_OK || state.event_count != 3u ||
        state.events[1][0] != RUNA_EVENT_MODULE_DATA || state.event_sizes[1] != 18u ||
        runa_read_u16_le(state.events[1] + 8u) != RUNA_GPIO_MODULE_ID ||
        runa_read_u16_le(state.events[1] + 10u) != 1u ||
        runa_read_u16_le(state.events[1] + 12u) != 0u ||
        runa_read_u16_le(state.events[1] + 14u) != 2u ||
        state.events[1][16] != 0xa5u || state.events[1][17] != 0u) return 17;

    memset(&state, 0, sizeof state);
    state.emit_data = 1u;
    job_size = make_job(job, valid_instructions, 3u, RUNA_IR_VERSION_V2);
    runa_write_u16_le(job + 34u, 0u);
    result = runa_process(job, job_size, &resources, &registry, &platform, &sink);
    if (result.error != RUNA_ERR_EMIT_LIMIT || state.writes != 0u || state.event_count != 2u ||
        state.events[0][0] != RUNA_EVENT_ACK || state.events[1][0] != RUNA_EVENT_RESULT) return 18;

    memset(&state, 0, sizeof state);
    job_size = make_job(job, valid_instructions, 3u, RUNA_IR_VERSION_V1);
    result = runa_process(job, job_size, &resources, &registry, &platform, &sink);
    if (result.error != RUNA_ERR_INVALID_OPCODE || result.accepted != 0u || state.writes != 0u ||
        state.event_count != 1u || state.events[0][0] != RUNA_EVENT_RESULT ||
        state.begins != 0u || state.ends != 0u) return 4;

    {
        static const uint8_t partial_instructions[] = {
            RUNA_OP_LOAD_CONST, 5u, 0u, 1u, 0u, 0u, 0u,
            RUNA_OP_EXT, 6u, 1u, 0u, RUNA_GPIO_OP_WRITE, 9u, 0u, 0u,
            RUNA_OP_LOAD_CONST, 5u, 0u, 1u, 0u, 0u, 0u,
            RUNA_OP_EXT, 6u, 1u, 0u, RUNA_GPIO_OP_WRITE, 9u, 0u, 0u,
            RUNA_OP_RETURN, 1u, 1u
        };
        memset(&state, 0, sizeof state);
        state.fail_write = 2u;
        state.fail_end = 1u;
        job_size = make_job(job, partial_instructions, 5u, RUNA_IR_VERSION_V2);
        result = runa_process(job, job_size, &resources, &registry, &platform, &sink);
        if (result.error != RUNA_GPIO_ERR_IO || result.accepted != 1u || result.result_sent != 1u ||
            state.output != 1u || state.writes != 2u || state.ends != 1u ||
            state.end_status != RUNA_GPIO_ERR_IO || state.event_count != 2u ||
            state.events[0][0] != RUNA_EVENT_ACK || state.events[1][0] != RUNA_EVENT_RESULT) return 5;
    }

    {
        static const uint8_t unknown_operation[] = {
            RUNA_OP_EXT, 6u, 1u, 0u, 0xffu, 9u, 0u, 0u,
            RUNA_OP_RETURN, 1u, 0u
        };
        memset(&state, 0, sizeof state);
        job_size = make_job(job, unknown_operation, 2u, RUNA_IR_VERSION_V2);
        result = runa_process(job, job_size, &resources, &registry, &platform, &sink);
        if (result.error != RUNA_ERR_INVALID_OPCODE || result.accepted != 0u ||
            state.writes != 0u || state.begins != 0u || state.event_count != 1u) return 6;
    }

    {
        static const uint8_t unknown_module[] = {
            RUNA_OP_EXT, 6u, 99u, 0u, RUNA_GPIO_OP_WRITE, 9u, 0u, 0u,
            RUNA_OP_RETURN, 1u, 0u
        };
        memset(&state, 0, sizeof state);
        job_size = make_job(job, unknown_module, 2u, RUNA_IR_VERSION_V2);
        result = runa_process(job, job_size, &resources, &registry, &platform, &sink);
        if (result.error != RUNA_ERR_UNSUPPORTED_MODULE || result.accepted != 0u ||
            state.writes != 0u || state.event_count != 1u) return 7;
    }

    {
        memset(&state, 0, sizeof state);
        state.fail_end = 1u;
        job_size = make_job(job, valid_instructions, 3u, RUNA_IR_VERSION_V2);
        result = runa_process(job, job_size, &resources, &registry, &platform, &sink);
        if (result.error != RUNA_ERR_INTERNAL || state.ends != 1u || state.end_status != RUNA_OK ||
            state.event_count != 2u || state.events[1][0] != RUNA_EVENT_RESULT) return 8;
    }

    puts("Runa V2 module dispatch checks passed");
    return 0;
}
