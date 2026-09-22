#include "runa_spi.h"
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
                              const runa_spi_resource_config_t *configuration,
                              const uint8_t *transmit, size_t transmit_size,
                              uint8_t *receive, size_t receive_size,
                              uint16_t timeout_ms) {
    fixture_t *state = (fixture_t *)context;
    size_t index;
    ++state->calls;
    if (handle != (uintptr_t)0x1234u || configuration == NULL || configuration->mode != 3u ||
        configuration->chip_select_behavior != RUNA_SPI_CS_SOFTWARE ||
        configuration->clock_hz != 1000000u || timeout_ms != 100u ||
        transmit_size != 3u || transmit == NULL || transmit[0] != 0x9fu ||
        transmit[1] != 0u || transmit[2] != 0u || receive_size != 51u || receive == NULL)
        return RUNA_SPI_ERR_IO;
    if (state->hal_status != RUNA_OK) return state->hal_status;
    for (index = 0u; index < receive_size; ++index) receive[index] = (uint8_t)index;
    return RUNA_OK;
}

static size_t make_job(uint8_t *job, uint8_t receive_size, uint16_t timeout_ms) {
    uint8_t *instruction;
    uint32_t instruction_bytes = 17u;
    memset(job, 0, RUNA_HEADER_SIZE + instruction_bytes);
    job[0] = (uint8_t)'J'; job[1] = (uint8_t)'E';
    job[2] = (uint8_t)'X'; job[3] = (uint8_t)'E';
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
    instruction[1] = 12u;
    runa_write_u16_le(instruction + 2u, RUNA_SPI_MODULE_ID);
    instruction[4] = RUNA_SPI_OP_TRANSFER;
    runa_write_u16_le(instruction + 5u, 5u);
    instruction[7] = 3u;
    instruction[8] = receive_size;
    runa_write_u16_le(instruction + 9u, timeout_ms);
    instruction[11] = 0x9fu;
    instruction[12] = 0u;
    instruction[13] = 0u;
    instruction[14] = RUNA_OP_RETURN;
    instruction[15] = 1u;
    instruction[16] = 0u;
    return RUNA_HEADER_SIZE + instruction_bytes;
}

static size_t make_adversarial_job(uint8_t *job) {
    size_t size = make_job(job, 1u, 100u);
    size_t invalid_offset = size - 3u;
    memmove(job + invalid_offset + 5u, job + invalid_offset, 3u);
    job[invalid_offset] = RUNA_OP_EXT;
    job[invalid_offset + 1u] = 3u;
    runa_write_u16_le(job + invalid_offset + 2u, RUNA_SPI_MODULE_ID);
    job[invalid_offset + 4u] = 99u;
    runa_write_u32_le(job + 12u, (uint32_t)(size + 5u));
    runa_write_u32_le(job + 16u, (uint32_t)(size + 5u - RUNA_HEADER_SIZE));
    runa_write_u16_le(job + 20u, 3u);
    return size + 5u;
}

int main(void) {
    fixture_t state = {0};
    runa_spi_resource_config_t configuration = {
        3u, RUNA_SPI_CS_SOFTWARE, 64u, 1000000u, 100000u, 10000000u, 1000u, 0u, 21u
    };
    runa_spi_hal_t hal = { &state, transfer };
    runa_module_t spi = runa_spi_module(&hal);
    runa_module_registry_t registry;
    runa_resource_t resource = { 5u, RUNA_SPI_MODULE_ID, RUNA_SPI_RESOURCE_TYPE, 0u,
                                 RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE,
                                 (uintptr_t)0x1234u, &configuration };
    runa_resource_table_t resources = { &resource, 1u };
    runa_platform_t platform = { &state, time_us, NULL };
    runa_event_sink_t sink = { capture, &state };
    runa_execution_summary_t result;
    uint8_t job[RUNA_MAX_JOB_BYTES];
    size_t job_size;
    uint8_t capability_bytes[128];
    size_t capability_size = 0u;
    runa_capabilities_view_t capability_view;

    runa_registry_init(&registry);
    {
        runa_status_t add_status = runa_registry_add(&registry, &spi);
        if (add_status != RUNA_OK) {
            (void)printf("SPI registry add failed: status=%u module=%u abi=%u validate=%u execute=%u resource=%u\\n",
                         (unsigned)add_status, (unsigned)spi.module_id, (unsigned)spi.abi_version,
                         (unsigned)(spi.validate != NULL), (unsigned)(spi.execute != NULL),
                         (unsigned)(spi.validate_resource != NULL));
            return 1;
        }
    }
    if (runa_capabilities_encode(&registry, capability_bytes, sizeof capability_bytes,
                                 &capability_size) != RUNA_OK ||
        runa_capabilities_decode(capability_bytes, capability_size, &capability_view) != RUNA_OK ||
        capability_view.module_count != 1u || capability_view.modules[0].module_id != RUNA_SPI_MODULE_ID ||
        capability_view.modules[0].payload_size != 4u ||
        capability_view.modules[0].payload[0] != RUNA_SPI_OP_TRANSFER ||
        runa_read_u16_le(capability_view.modules[0].payload + 1u) != RUNA_SPI_MAX_TRANSFER_BYTES ||
        capability_view.modules[0].payload[3] != 0x0fu) return 7;
    job_size = make_job(job, 51u, 100u);
    result = runa_process(job, job_size, &resources, &registry, &platform, &sink);
    if (result.error != RUNA_OK || state.calls != 1u || state.event_count != 4u ||
        state.events[0][0] != RUNA_EVENT_ACK || state.events[1][0] != RUNA_EVENT_MODULE_DATA ||
        state.events[2][0] != RUNA_EVENT_MODULE_DATA || state.events[3][0] != RUNA_EVENT_RESULT)
        return 2;
    if (state.event_sizes[1] != 64u || state.event_sizes[2] != 19u ||
        runa_read_u16_le(state.events[1] + 8u) != RUNA_SPI_MODULE_ID ||
        runa_read_u16_le(state.events[1] + 10u) != 0u ||
        runa_read_u16_le(state.events[1] + 12u) != 0u ||
        runa_read_u16_le(state.events[1] + 14u) != 48u ||
        state.events[1][16] != 0u || state.events[1][63] != 47u ||
        runa_read_u16_le(state.events[2] + 12u) != 1u ||
        runa_read_u16_le(state.events[2] + 14u) != 3u ||
        state.events[2][16] != 48u || state.events[2][18] != 50u) return 3;

    memset(&state, 0, sizeof state);
    job_size = make_job(job, 65u, 100u);
    result = runa_process(job, job_size, &resources, &registry, &platform, &sink);
    if (result.error != RUNA_ERR_OUT_OF_RANGE || state.calls != 0u || state.event_count != 1u ||
        state.events[0][0] != RUNA_EVENT_RESULT) return 4;

    memset(&state, 0, sizeof state);
    state.hal_status = RUNA_ERR_IO_TIMEOUT;
    job_size = make_job(job, 51u, 100u);
    result = runa_process(job, job_size, &resources, &registry, &platform, &sink);
    if (result.error != RUNA_ERR_IO_TIMEOUT || state.calls != 1u || state.event_count != 2u ||
        state.events[0][0] != RUNA_EVENT_ACK || state.events[1][0] != RUNA_EVENT_RESULT) return 5;

    resource.permissions = RUNA_PERMISSION_WRITE;
    memset(&state, 0, sizeof state);
    job_size = make_job(job, 51u, 100u);
    result = runa_process(job, job_size, &resources, &registry, &platform, &sink);
    if (result.error != RUNA_ERR_ACCESS_DENIED || state.calls != 0u || state.event_count != 1u)
        return 6;

    resource.permissions = RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE;
    for (unsigned case_index = 0u; case_index < 1000u; ++case_index) {
        memset(&state, 0, sizeof state);
        result = runa_process(job, make_adversarial_job(job), &resources, &registry, &platform, &sink);
        if (result.error == RUNA_OK || state.calls != 0u) return 8;
    }
    resource.permissions = RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE;
    for (unsigned case_index = 0u; case_index < 10000u; ++case_index) {
        unsigned variant = case_index % 7u;
        memset(&state, 0, sizeof state);
        if (variant == 0u) {
            result = runa_process(job, make_job(job, 51u, 100u), &resources, &registry,
                                   &platform, &sink);
            if (result.error != RUNA_OK || state.calls != 1u) return 9;
        } else {
            uint8_t rx = variant == 1u || variant >= 3u ? 65u : 1u;
            uint16_t timeout = variant == 2u ? 0u : 100u;
            result = runa_process(job, make_job(job, rx, timeout), &resources, &registry,
                                   &platform, &sink);
            if (result.error == RUNA_OK || state.calls != 0u) return 10;
        }
    }

    puts("Runa.SPI bounded transfer checks passed");
    return 0;
}
