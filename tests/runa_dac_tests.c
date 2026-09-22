#include "runa_dac.h"
#include "runa_capabilities.h"
#include "runa_ir.h"
#include "runa_platform.h"
#include "runa_registry.h"
#include "runa_resource.h"
#include "runa_result.h"
#include "runa_runtime.h"

#include <stdio.h>
#include <string.h>

typedef struct state { uint32_t calls; uintptr_t handle; uint32_t value; runa_status_t next; uint8_t events[4]; uint8_t count; } state_t;
static runa_status_t write_mock(void *context, uintptr_t handle, uint32_t value) {
    state_t *state = (state_t *)context; runa_status_t status = state->next; state->next = RUNA_OK; ++state->calls;
    if (status == RUNA_OK) { state->handle = handle; state->value = value; } return status;
}
static int sink(void *context, const uint8_t *data, size_t size) {
    state_t *state = (state_t *)context; if (data == NULL || size == 0u || state->count >= 4u) return -1;
    state->events[state->count++] = data[0]; return 0;
}
static uint64_t time_us(void *context) { (void)context; return 0u; }
static runa_status_t delay_ms(void *context, uint32_t value) { (void)context; (void)value; return RUNA_OK; }
static size_t make_job(uint8_t *job, uint32_t value, uint8_t operation, uint8_t reg, uint8_t ext_size) {
    uint8_t *p = job + RUNA_HEADER_SIZE; size_t instruction_size;
    memset(job, 0, 128u); job[0] = 'J'; job[1] = 'E'; job[2] = 'X'; job[3] = 'E'; job[4] = 1u; job[5] = 2u;
    runa_write_u16_le(job + 6u, RUNA_HEADER_SIZE); runa_write_u32_le(job + 8u, 0x10203040u);
    runa_write_u32_le(job + 24u, 100u); runa_write_u32_le(job + 28u, 1000u);
    runa_write_u16_le(job + 32u, 32u); runa_write_u16_le(job + 34u, 8u); runa_write_u32_le(job + 36u, 512u);
    p[0] = RUNA_OP_LOAD_CONST; p[1] = 5u; p[2] = 0u; runa_write_u32_le(p + 3u, value); p += 7u;
    p[0] = RUNA_OP_EXT; p[1] = ext_size; runa_write_u16_le(p + 2u, RUNA_DAC_MODULE_ID); p[4] = operation;
    if (ext_size >= 5u) {
        runa_write_u16_le(p + 5u, 9u);
    }
    if (ext_size >= 6u) {
        p[7] = reg;
    }
    p += 2u + ext_size;
    p[0] = RUNA_OP_RETURN; p[1] = 1u; p[2] = 0u; p += 3u; instruction_size = (size_t)(p - (job + RUNA_HEADER_SIZE));
    runa_write_u32_le(job + 12u, (uint32_t)(RUNA_HEADER_SIZE + instruction_size)); runa_write_u32_le(job + 16u, (uint32_t)instruction_size);
    runa_write_u16_le(job + 20u, 3u); return RUNA_HEADER_SIZE + instruction_size;
}
static size_t make_adversarial_job(uint8_t *job, uint32_t value) {
    size_t size = make_job(job, value, RUNA_DAC_OP_WRITE, 0u, 6u);
    size_t invalid_offset = size - 3u;
    memmove(job + invalid_offset + 5u, job + invalid_offset, 3u);
    job[invalid_offset] = RUNA_OP_EXT; job[invalid_offset + 1u] = 3u;
    runa_write_u16_le(job + invalid_offset + 2u, RUNA_DAC_MODULE_ID); job[invalid_offset + 4u] = 99u;
    runa_write_u32_le(job + 12u, (uint32_t)(size + 5u));
    runa_write_u32_le(job + 16u, (uint32_t)(size + 5u - RUNA_HEADER_SIZE));
    runa_write_u16_le(job + 20u, 4u);
    return size + 5u;
}
static runa_execution_summary_t run_job(state_t *state, const runa_dac_resource_config_t *config,
                                         uint32_t permissions, uint32_t value, uint8_t operation,
                                         uint8_t reg, uint8_t ext_size) {
    uint8_t job[128]; size_t size = make_job(job, value, operation, reg, ext_size);
    runa_dac_hal_t hal = { state, write_mock }; runa_module_t module = runa_dac_module(&hal); runa_module_registry_t registry;
    runa_resource_t item = { 9u, RUNA_DAC_MODULE_ID, RUNA_DAC_RESOURCE_TYPE, 0u, permissions, 0x55u, config };
    runa_resource_table_t resources = { &item, 1u }; runa_platform_t platform = { state, time_us, delay_ms }; runa_event_sink_t events = { sink, state };
    runa_registry_init(&registry); (void)runa_registry_add(&registry, &module);
    return runa_process(job, size, &resources, &registry, &platform, &events);
}
static int valid(state_t *state, const runa_dac_resource_config_t *config, uint32_t value) {
    runa_execution_summary_t summary; memset(state, 0, sizeof *state); summary = run_job(state, config, RUNA_PERMISSION_WRITE, value, 1u, 0u, 6u);
    return summary.error == RUNA_OK && summary.accepted == 1u && summary.result_sent == 1u && state->count == 2u &&
           state->events[0] == RUNA_EVENT_ACK && state->events[1] == RUNA_EVENT_RESULT && state->calls == 1u &&
           state->handle == 0x55u && state->value == value;
}
int main(void) {
    const runa_dac_resource_config_t configs[] = {
        {255u, 8u, {0u, 0u, 0u}}, {1023u, 10u, {0u, 0u, 0u}}, {4095u, 12u, {0u, 0u, 0u}},
        {65535u, 16u, {0u, 0u, 0u}}, {UINT32_MAX, 32u, {0u, 0u, 0u}}
    };
    state_t state; size_t i; runa_capabilities_view_t view; uint8_t bytes[RUNA_MAX_CAPABILITY_BYTES]; size_t written;
    runa_dac_hal_t hal = { &state, write_mock }; runa_module_t module = runa_dac_module(&hal); runa_module_registry_t registry;
    for (i = 0u; i < sizeof configs / sizeof configs[0]; ++i) {
        if (!valid(&state, &configs[i], 0u) || !valid(&state, &configs[i], 1u) ||
            !valid(&state, &configs[i], configs[i].maximum_value - 1u) || !valid(&state, &configs[i], configs[i].maximum_value)) return 1;
        if (configs[i].maximum_value != UINT32_MAX) { runa_execution_summary_t summary; memset(&state, 0, sizeof state);
            summary = run_job(&state, &configs[i], RUNA_PERMISSION_WRITE, configs[i].maximum_value + 1u, 1u, 0u, 6u);
            if (summary.error != RUNA_ERR_OUT_OF_RANGE || summary.accepted != 1u || state.calls != 0u) return 2; }
    }
    { runa_execution_summary_t summary; memset(&state, 0, sizeof state); summary = run_job(&state, &configs[0], RUNA_PERMISSION_READ, 1u, 1u, 0u, 6u);
      if (summary.error != RUNA_ERR_ACCESS_DENIED || summary.accepted != 0u || state.count != 1u || state.calls != 0u) return 3; }
    { runa_execution_summary_t summary; memset(&state, 0, sizeof state); summary = run_job(&state, &configs[0], RUNA_PERMISSION_WRITE, 1u, 0xffu, 0u, 6u);
      if (summary.error != RUNA_ERR_INVALID_OPCODE || state.calls != 0u) return 4; }
    { runa_execution_summary_t summary; memset(&state, 0, sizeof state); summary = run_job(&state, &configs[0], RUNA_PERMISSION_WRITE, 1u, 1u, 8u, 6u);
      if (summary.error != RUNA_ERR_INVALID_REGISTER || state.calls != 0u) return 5; }
    { runa_execution_summary_t summary; memset(&state, 0, sizeof state); summary = run_job(&state, &configs[0], RUNA_PERMISSION_WRITE, 1u, 1u, 0u, 5u);
      if (summary.error != RUNA_ERR_INVALID_OPERAND || state.calls != 0u) return 6; }
    memset(&state, 0, sizeof state); state.next = RUNA_DAC_ERR_IO;
    if (run_job(&state, &configs[0], RUNA_PERMISSION_WRITE, 7u, 1u, 0u, 6u).error != RUNA_DAC_ERR_IO || state.calls != 1u || state.value != 0u) return 7;
    if (!valid(&state, &configs[0], 8u)) return 8;
    runa_registry_init(&registry);
    if (runa_registry_add(&registry, &module) != RUNA_OK || runa_capabilities_encode(&registry, bytes, sizeof bytes, &written) != RUNA_OK ||
        runa_capabilities_decode(bytes, written, &view) != RUNA_OK || view.module_count != 1u || view.modules[0].module_id != 9u ||
        view.modules[0].payload_size != 4u || memcmp(view.modules[0].payload, (uint8_t[]){1u, 0u, 32u, 0u}, 4u) != 0) return 9;
    { runa_dac_resource_config_t bad = {0u, 8u, {0u, 0u, 0u}}; runa_resource_t item = {9u, 9u, 1u, 0u, 2u, 0u, &bad}; runa_resource_table_t table = {&item, 1u};
      if (runa_resource_table_validate(&table, &registry) != RUNA_ERR_INVALID_RESOURCE) return 10; }
    for (size_t case_index = 0u; case_index < 5000u; ++case_index) {
        const runa_dac_resource_config_t *config = &configs[case_index % (sizeof configs / sizeof configs[0])];
        uint32_t value = case_index % 7u == 0u ? config->maximum_value :
                         (uint32_t)(case_index % config->maximum_value);
        if (!valid(&state, config, value)) return 11;
    }
    for (size_t case_index = 0u; case_index < 1000u; ++case_index) {
        const runa_dac_resource_config_t *config = &configs[case_index % (sizeof configs / sizeof configs[0])];
        runa_execution_summary_t summary; uint8_t job[128];
        runa_dac_hal_t test_hal = { &state, write_mock }; runa_module_t test_module = runa_dac_module(&test_hal);
        runa_module_registry_t test_registry; runa_resource_t item = { 9u, RUNA_DAC_MODULE_ID,
            RUNA_DAC_RESOURCE_TYPE, 0u, RUNA_PERMISSION_WRITE, 0x55u, config };
        runa_resource_table_t table = { &item, 1u }; runa_platform_t platform = { &state, time_us, delay_ms };
        runa_event_sink_t events = { sink, &state };
        runa_registry_init(&test_registry); (void)runa_registry_add(&test_registry, &test_module);
        memset(&state, 0, sizeof state);
        uint32_t generated_value = config->maximum_value == UINT32_MAX ? (uint32_t)case_index :
                                   (uint32_t)(case_index % (config->maximum_value + 1u));
        summary = runa_process(job, make_adversarial_job(job, generated_value),
                               &table, &test_registry, &platform, &events);
        if (summary.error == RUNA_OK || state.calls != 0u) return 12;
    }
    puts("Runa.DAC focused checks passed"); return 0;
}
