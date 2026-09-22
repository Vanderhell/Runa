#include "runa_onewire.h"
#include "runa_capabilities.h"
#include "runa_ir.h"
#include "runa_runtime.h"

#include <stdio.h>
#include <string.h>

typedef struct fixture {
    uint8_t events[64][RUNA_MAX_EVENT_BYTES];
    size_t event_sizes[64];
    uint8_t event_count;
    uint8_t presence;
    uint8_t reset_calls;
    uint8_t transfer_calls;
    uint8_t search_calls;
    runa_status_t hal_status;
    uint8_t last_rom[8];
    uint8_t last_rom_was_skip;
    uint8_t last_tx[16];
    size_t last_tx_size;
    uint8_t rx[64];
    size_t rx_size;
    uint8_t roms[RUNA_ONEWIRE_MAX_SEARCH_RESULTS][8];
    size_t rom_count;
} fixture_t;

static int capture(void *context, const uint8_t *event, size_t size) {
    fixture_t *fixture = (fixture_t *)context;
    if (fixture == NULL || event == NULL || fixture->event_count >= 64u ||
        size > RUNA_MAX_EVENT_BYTES) return -1;
    memcpy(fixture->events[fixture->event_count], event, size);
    fixture->event_sizes[fixture->event_count] = size;
    ++fixture->event_count;
    return 0;
}

static uint64_t time_us(void *context) { (void)context; return 0u; }

static runa_status_t mock_reset(void *context, uintptr_t handle, uint32_t timeout_us,
                                uint8_t *presence) {
    fixture_t *fixture = (fixture_t *)context;
    if (fixture == NULL || handle != (uintptr_t)0x1234u || timeout_us != 1000u ||
        presence == NULL) return RUNA_ERR_INTERNAL;
    ++fixture->reset_calls;
    *presence = fixture->presence;
    return fixture->hal_status;
}

static runa_status_t mock_transfer(void *context, uintptr_t handle, const uint8_t *rom_id,
                                   const uint8_t *transmit, size_t transmit_size,
                                   uint8_t *receive, size_t receive_size, uint32_t timeout_us,
                                   uint32_t strong_pullup_us) {
    fixture_t *fixture = (fixture_t *)context;
    if (fixture == NULL || handle != (uintptr_t)0x1234u || timeout_us != 1000u ||
        strong_pullup_us != 0u || transmit_size > sizeof fixture->last_tx ||
        (transmit_size != 0u && transmit == NULL) ||
        (receive_size != 0u && receive == NULL)) return RUNA_ERR_INTERNAL;
    ++fixture->transfer_calls;
    fixture->last_rom_was_skip = rom_id == NULL ? 1u : 0u;
    if (rom_id != NULL) memcpy(fixture->last_rom, rom_id, sizeof fixture->last_rom);
    fixture->last_tx_size = transmit_size;
    if (transmit_size != 0u) memcpy(fixture->last_tx, transmit, transmit_size);
    if (fixture->hal_status != RUNA_OK) return fixture->hal_status;
    if (receive_size > fixture->rx_size) return RUNA_ERR_INTERNAL;
    if (receive_size != 0u) memcpy(receive, fixture->rx, receive_size);
    return RUNA_OK;
}

static runa_status_t mock_search(void *context, uintptr_t handle, uint8_t *rom_ids,
                                 size_t rom_ids_capacity, size_t maximum_results,
                                 size_t *result_count, uint32_t timeout_us) {
    fixture_t *fixture = (fixture_t *)context;
    size_t count;
    if (fixture == NULL || handle != (uintptr_t)0x1234u || rom_ids == NULL ||
        result_count == NULL || timeout_us != 1000u || maximum_results > RUNA_ONEWIRE_MAX_SEARCH_RESULTS)
        return RUNA_ERR_INTERNAL;
    ++fixture->search_calls;
    if (fixture->hal_status != RUNA_OK) return fixture->hal_status;
    count = fixture->rom_count < maximum_results ? fixture->rom_count : maximum_results;
    if (count * 8u > rom_ids_capacity) return RUNA_ERR_INTERNAL;
    memcpy(rom_ids, fixture->roms, count * 8u);
    *result_count = count;
    return RUNA_OK;
}

static size_t append_instruction(uint8_t *job, size_t offset, uint8_t opcode,
                                 const uint8_t *operands, uint8_t operand_size) {
    job[offset] = opcode;
    job[offset + 1u] = operand_size;
    if (operand_size != 0u) memcpy(job + offset + 2u, operands, operand_size);
    return offset + 2u + operand_size;
}

static void finish_job(uint8_t *job, size_t size, uint16_t instruction_count) {
    job[0] = 'J'; job[1] = 'E'; job[2] = 'X'; job[3] = 'E';
    job[4] = RUNA_PROTOCOL_VERSION; job[5] = RUNA_IR_VERSION_V2;
    runa_write_u16_le(job + 6u, RUNA_HEADER_SIZE);
    runa_write_u32_le(job + 8u, 0x10203040u);
    runa_write_u32_le(job + 12u, (uint32_t)size);
    runa_write_u32_le(job + 16u, (uint32_t)(size - RUNA_HEADER_SIZE));
    runa_write_u16_le(job + 20u, instruction_count);
    runa_write_u16_le(job + 22u, 0u);
    runa_write_u32_le(job + 24u, 100u);
    runa_write_u32_le(job + 28u, 1000000u);
    runa_write_u16_le(job + 32u, RUNA_MAX_RESULT_BYTES);
    runa_write_u16_le(job + 34u, 32u);
    runa_write_u32_le(job + 36u, RUNA_MAX_EMIT_BYTES);
}

static size_t one_wire_extension(uint8_t *job, size_t offset, uint8_t operation,
                                 const uint8_t *payload, uint8_t payload_size) {
    uint8_t operands[UINT8_MAX];
    runa_write_u16_le(operands, RUNA_ONEWIRE_MODULE_ID);
    operands[2] = operation;
    memcpy(operands + 3u, payload, payload_size);
    return append_instruction(job, offset, RUNA_OP_EXT, operands, (uint8_t)(payload_size + 3u));
}

static size_t make_reset(uint8_t *job, uint16_t resource_id) {
    uint8_t payload[7] = {0u};
    size_t offset = RUNA_HEADER_SIZE;
    runa_write_u16_le(payload, resource_id); runa_write_u32_le(payload + 2u, 1000u); payload[6] = 0u;
    offset = one_wire_extension(job, offset, RUNA_ONEWIRE_OP_RESET, payload, sizeof payload);
    offset = append_instruction(job, offset, RUNA_OP_RETURN, (const uint8_t *)"\x01", 1u);
    finish_job(job, offset, 2u);
    return offset;
}

static size_t make_transfer(uint8_t *job, uint16_t resource_id, uint8_t flags,
                            const uint8_t *transmit, uint8_t transmit_size, uint8_t receive_size) {
    uint8_t payload[9u + RUNA_ONEWIRE_MAX_TX_BYTES] = {0u};
    uint8_t mask = 0u;
    size_t offset = RUNA_HEADER_SIZE;
    runa_write_u16_le(payload, resource_id); payload[2] = flags; payload[3] = transmit_size;
    payload[4] = receive_size; runa_write_u32_le(payload + 5u, 1000u);
    if (transmit_size != 0u) memcpy(payload + 9u, transmit, transmit_size);
    offset = one_wire_extension(job, offset, RUNA_ONEWIRE_OP_TRANSFER, payload,
                                (uint8_t)(9u + transmit_size));
    offset = append_instruction(job, offset, RUNA_OP_RETURN, &mask, 1u);
    finish_job(job, offset, 2u);
    return offset;
}

static size_t make_adversarial_transfer(uint8_t *job, uint16_t resource_id,
                                        const uint8_t *transmit, uint8_t transmit_size) {
    size_t size = make_transfer(job, resource_id, 0u, transmit, transmit_size, 1u);
    size_t invalid_offset = size - 3u;
    memmove(job + invalid_offset + 5u, job + invalid_offset, 3u);
    job[invalid_offset] = RUNA_OP_EXT; job[invalid_offset + 1u] = 3u;
    runa_write_u16_le(job + invalid_offset + 2u, RUNA_ONEWIRE_MODULE_ID);
    job[invalid_offset + 4u] = 99u;
    finish_job(job, size + 5u, 3u);
    return size + 5u;
}

static size_t make_search(uint8_t *job, uint16_t resource_id, uint8_t maximum_results) {
    uint8_t payload[7] = {0u};
    uint8_t mask = 0u;
    size_t offset = RUNA_HEADER_SIZE;
    runa_write_u16_le(payload, resource_id); payload[2] = maximum_results;
    runa_write_u32_le(payload + 3u, 1000u);
    offset = one_wire_extension(job, offset, RUNA_ONEWIRE_OP_ROM_SEARCH, payload, sizeof payload);
    offset = append_instruction(job, offset, RUNA_OP_RETURN, &mask, 1u);
    finish_job(job, offset, 2u);
    return offset;
}

static int result_status(const fixture_t *fixture, runa_status_t status) {
    return fixture->event_count > 0u && fixture->events[fixture->event_count - 1u][0] == RUNA_EVENT_RESULT &&
           fixture->events[fixture->event_count - 1u][9] == status;
}

int main(void) {
    fixture_t fixture = {0};
    runa_onewire_bus_resource_config_t bus_configuration = { 32u, 32u, 4u, 0u, 1u, 1u, 0u, 5000000u };
    runa_onewire_device_resource_config_t device_configuration = { 7u, 0u, { 0x28u, 1u, 2u, 3u, 4u, 5u, 6u, 0u } };
    runa_onewire_hal_t hal = { &fixture, 0u, {0u, 0u, 0u}, mock_reset, mock_transfer, mock_search };
    runa_module_t module = runa_onewire_module(&hal);
    runa_module_registry_t registry;
    runa_resource_t resources_array[2];
    runa_resource_table_t resources = { resources_array, 2u };
    runa_platform_t platform = { &fixture, time_us, NULL };
    runa_event_sink_t sink = { capture, &fixture };
    uint8_t job[RUNA_MAX_JOB_BYTES];
    uint8_t tx[2] = { 0xaau, 0x55u };
    uint8_t capability[64];
    size_t capability_size;
    runa_capabilities_view_t capability_view;
    runa_execution_summary_t summary;

    device_configuration.rom_id[7] = runa_onewire_rom_crc8(device_configuration.rom_id);
    resources_array[0] = (runa_resource_t){ 7u, RUNA_ONEWIRE_MODULE_ID,
        RUNA_ONEWIRE_BUS_RESOURCE_TYPE, 0u, RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE,
        (uintptr_t)0x1234u, &bus_configuration };
    resources_array[1] = (runa_resource_t){ 8u, RUNA_ONEWIRE_MODULE_ID,
        RUNA_ONEWIRE_DEVICE_RESOURCE_TYPE, 0u, RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE,
        (uintptr_t)0x1234u, &device_configuration };
    runa_registry_init(&registry);
    if (runa_registry_add(&registry, &module) != RUNA_OK ||
        runa_resource_table_validate(&resources, &registry) != RUNA_OK ||
        runa_capabilities_encode(&registry, capability, sizeof capability, &capability_size) != RUNA_OK ||
        runa_capabilities_decode(capability, capability_size, &capability_view) != RUNA_OK ||
        capability_view.module_count != 1u || capability_view.modules[0].module_id != 11u ||
        capability_view.modules[0].payload_size != RUNA_ONEWIRE_CAPABILITY_SIZE) return 1;

    fixture.presence = 1u;
    summary = runa_process(job, make_reset(job, 7u), &resources, &registry, &platform, &sink);
    if (summary.error != RUNA_OK || fixture.reset_calls != 1u || fixture.event_count != 2u ||
        fixture.events[1][0] != RUNA_EVENT_RESULT || fixture.events[1][20] != 1u) {
        printf("reset error=%u calls=%u events=%u type=%u value=%u detail=%u instr=%u\\n", summary.error,
               fixture.reset_calls, fixture.event_count, fixture.events[0][0], fixture.events[0][20],
               runa_read_u32_le(fixture.events[0] + 12u), runa_read_u16_le(fixture.events[0] + 10u));
        return 2;
    }
    memset(&fixture, 0, sizeof fixture); fixture.presence = 0u;
    summary = runa_process(job, make_reset(job, 7u), &resources, &registry, &platform, &sink);
    if (summary.error != RUNA_OK || fixture.reset_calls != 1u || !result_status(&fixture, RUNA_OK) ||
        fixture.events[1][20] != 0u) return 3;

    memset(&fixture, 0, sizeof fixture); fixture.rx_size = 4u;
    fixture.rx[0] = 9u; fixture.rx[1] = 8u; fixture.rx[2] = 7u; fixture.rx[3] = 6u;
    summary = runa_process(job, make_transfer(job, 8u, 0u, tx, 2u, 4u),
                           &resources, &registry, &platform, &sink);
    if (summary.error != RUNA_OK || fixture.transfer_calls != 1u || fixture.last_rom_was_skip != 0u ||
        fixture.last_tx_size != 2u || memcmp(fixture.last_tx, tx, 2u) != 0 || fixture.event_count != 3u ||
        fixture.events[1][0] != RUNA_EVENT_MODULE_DATA || fixture.events[1][16] != 9u) return 4;

    memset(&fixture, 0, sizeof fixture);
    fixture.rom_count = 2u;
    memcpy(fixture.roms[0], device_configuration.rom_id, 8u);
    memcpy(fixture.roms[1], device_configuration.rom_id, 8u); fixture.roms[1][1] = 9u;
    fixture.roms[1][7] = runa_onewire_rom_crc8(fixture.roms[1]);
    summary = runa_process(job, make_search(job, 7u, 2u), &resources, &registry, &platform, &sink);
    if (summary.error != RUNA_OK || fixture.search_calls != 1u || fixture.event_count != 4u ||
        fixture.events[1][0] != RUNA_EVENT_MODULE_DATA || fixture.events[2][17] != 9u) return 5;

    memset(&fixture, 0, sizeof fixture);
    summary = runa_process(job, make_transfer(job, 7u, 0u, tx, 2u, 0u),
                           &resources, &registry, &platform, &sink);
    if (summary.error != RUNA_ERR_ACCESS_DENIED || fixture.transfer_calls != 0u || fixture.event_count != 1u) return 6;
    memset(&fixture, 0, sizeof fixture);
    fixture.rom_count = 1u; memcpy(fixture.roms[0], device_configuration.rom_id, 8u); fixture.roms[0][7] ^= 1u;
    summary = runa_process(job, make_search(job, 7u, 1u), &resources, &registry, &platform, &sink);
    if (summary.error != RUNA_ONEWIRE_ERR_CRC || fixture.event_count != 2u) return 7;

    memset(&fixture, 0, sizeof fixture);
    {
        uint8_t bad[4] = { 7u, 0u, 0u, 0u };
        size_t offset = one_wire_extension(job, RUNA_HEADER_SIZE, RUNA_ONEWIRE_OP_TRANSFER, bad, sizeof bad);
        uint8_t invalid[1] = { 0u };
        offset = one_wire_extension(job, offset, 99u, invalid, sizeof invalid);
        finish_job(job, offset, 2u);
        summary = runa_process(job, offset, &resources, &registry, &platform, &sink);
    }
    if (summary.error != RUNA_ERR_INVALID_OPERAND || fixture.transfer_calls != 0u || fixture.event_count != 1u) return 8;
    for (unsigned case_index = 0u; case_index < 1000u; ++case_index) {
        memset(&fixture, 0, sizeof fixture);
        summary = runa_process(job, make_adversarial_transfer(job, 8u, tx,
                                                              (uint8_t)(case_index % 3u)),
                               &resources, &registry, &platform, &sink);
        if (summary.error == RUNA_OK || fixture.transfer_calls != 0u ||
            fixture.reset_calls != 0u || fixture.search_calls != 0u) return 9;
    }
    puts("Runa.OneWire bounded reset/transfer/search checks passed");
    return 0;
}
