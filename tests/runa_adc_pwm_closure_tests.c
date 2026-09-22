#include "runa_adc.h"
#include "runa_pwm.h"
#include "runa_ir.h"
#include "runa_runtime.h"

#include <stdio.h>
#include <string.h>

typedef struct closure_state { uint32_t calls; uint32_t value; } closure_state_t;
static runa_status_t adc_read(void *context, uintptr_t handle, uint32_t *value) {
    closure_state_t *state = (closure_state_t *)context; (void)handle; ++state->calls; *value = state->value; return RUNA_OK;
}
static runa_status_t pwm_write(void *context, uintptr_t handle, uint32_t value) {
    closure_state_t *state = (closure_state_t *)context; (void)handle; ++state->calls; state->value = value; return RUNA_OK;
}
static int sink(void *context, const uint8_t *data, size_t size) { (void)context; (void)data; (void)size; return 0; }
static uint64_t time_us(void *context) { (void)context; return 0u; }

static size_t make_job(uint8_t *job, uint16_t module_id, uint8_t operation,
                       uint32_t value, uint8_t invalid_kind) {
    uint8_t *p = job + RUNA_HEADER_SIZE;
    size_t instruction_bytes = 7u + 8u + 5u + 3u;
    memset(job, 0, RUNA_MAX_JOB_BYTES);
    job[0] = 'J'; job[1] = 'E'; job[2] = 'X'; job[3] = 'E'; job[4] = 1u; job[5] = 2u;
    runa_write_u16_le(job + 6u, RUNA_HEADER_SIZE); runa_write_u32_le(job + 8u, 0x90000000u + invalid_kind);
    runa_write_u32_le(job + 12u, (uint32_t)(RUNA_HEADER_SIZE + instruction_bytes));
    runa_write_u32_le(job + 16u, (uint32_t)instruction_bytes); runa_write_u16_le(job + 20u, 4u);
    runa_write_u32_le(job + 24u, RUNA_MAX_STEPS); runa_write_u32_le(job + 28u, RUNA_MAX_RUNTIME_US);
    runa_write_u16_le(job + 32u, RUNA_MAX_RESULT_BYTES); runa_write_u16_le(job + 34u, RUNA_MAX_EMITS);
    runa_write_u32_le(job + 36u, RUNA_MAX_EMIT_BYTES);
    p[0] = RUNA_OP_LOAD_CONST; p[1] = 5u; p[2] = 0u; runa_write_u32_le(p + 3u, value); p += 7u;
    p[0] = RUNA_OP_EXT; p[1] = 6u; runa_write_u16_le(p + 2u, module_id); p[4] = operation;
    runa_write_u16_le(p + 5u, 1u); p[7] = 0u; p += 8u;
    p[0] = RUNA_OP_EXT; p[1] = invalid_kind == 2u ? 2u : 3u;
    runa_write_u16_le(p + 2u, invalid_kind == 1u ? 99u : module_id); p[4] = invalid_kind == 3u ? 99u : operation;
    p += 5u; p[0] = RUNA_OP_RETURN; p[1] = 1u; p[2] = 0u;
    return RUNA_HEADER_SIZE + instruction_bytes;
}

static int run_matrix(uint16_t module_id, uint8_t operation, const runa_resource_t *resource,
                      runa_module_t *module, uint32_t cases) {
    runa_module_registry_t registry; runa_resource_table_t resources = { resource, 1u };
    runa_platform_t platform = { NULL, time_us, NULL }; runa_event_sink_t events = { sink, NULL };
    uint8_t job[RUNA_MAX_JOB_BYTES]; closure_state_t state = { 0u, 0u };
    runa_status_t status; uint32_t i;
    platform.context = &state; module->context = module->context;
    runa_registry_init(&registry); status = runa_registry_add(&registry, module); if (status != RUNA_OK) return 1;
    for (i = 0u; i < cases; ++i) {
        runa_execution_summary_t summary; state.calls = 0u; state.value = i & 1u;
        summary = runa_process(job, make_job(job, module_id, operation, state.value,
                                             (uint8_t)(1u + (i % 3u))), &resources, &registry,
                                &platform, &events);
        if (summary.error == RUNA_OK || state.calls != 0u) return 2;
    }
    return 0;
}

int main(void) {
    closure_state_t adc_state = { 0u, 4095u }, pwm_state = { 0u, 0u };
    runa_adc_hal_t adc_hal = { &adc_state, adc_read }; runa_module_t adc = runa_adc_module(&adc_hal);
    runa_pwm_hal_t pwm_hal = { &pwm_state, pwm_write }; runa_module_t pwm = runa_pwm_module(&pwm_hal);
    runa_adc_resource_config_t adc_config = { 4095u }; runa_pwm_resource_config_t pwm_config = { 10000u };
    runa_resource_t adc_resource = { 1u, RUNA_ADC_MODULE_ID, RUNA_ADC_RESOURCE_TYPE, 0u,
                                     RUNA_PERMISSION_READ, 1u, &adc_config };
    runa_resource_t pwm_resource = { 1u, RUNA_PWM_MODULE_ID, RUNA_PWM_RESOURCE_TYPE, 0u,
                                     RUNA_PERMISSION_WRITE, 1u, &pwm_config };
    if (run_matrix(RUNA_ADC_MODULE_ID, RUNA_ADC_OP_READ, &adc_resource, &adc, 1000u) != 0) return 1;
    if (run_matrix(RUNA_PWM_MODULE_ID, RUNA_PWM_OP_WRITE, &pwm_resource, &pwm, 1000u) != 0) return 2;
    puts("Runa ADC/PWM closure isolation passed"); return 0;
}
