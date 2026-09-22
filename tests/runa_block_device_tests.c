#include "runa_block_device.h"
#include "runa_capabilities.h"
#include "runa_ir.h"
#include "runa_runtime.h"
#include "runa_validator.h"
#include "runa_block_device_mock_hal.h"

#include <stdio.h>
#include <string.h>

typedef struct fixture {
    uint8_t events[16][RUNA_MAX_EVENT_BYTES];
    size_t sizes[16];
    uint8_t count;
} fixture_t;

static int capture(void *context, const uint8_t *event, size_t size) {
    fixture_t *fixture = (fixture_t *)context;
    if (fixture == NULL || event == NULL || fixture->count >= 16u ||
        size > RUNA_MAX_EVENT_BYTES) return -1;
    memcpy(fixture->events[fixture->count], event, size);
    fixture->sizes[fixture->count] = size;
    ++fixture->count;
    return 0;
}

static uint64_t time_us(void *context) { (void)context; return 0u; }

static int check(int condition, const char *name) {
    if (!condition) {
        (void)printf("BlockDevice check failed: %s\n", name);
        return 0;
    }
    return 1;
}

static size_t make_job(uint8_t *job, uint32_t id, uint8_t operation, uint16_t resource_id,
                       uint32_t offset, uint32_t length, const uint8_t *data,
                       size_t data_size, int with_return) {
    uint8_t *instruction = job + RUNA_HEADER_SIZE;
    uint8_t operand_size;
    size_t instruction_bytes;
    size_t total;
    size_t cursor;
    memset(job, 0, RUNA_MAX_JOB_BYTES);
    if (operation == RUNA_BLOCK_DEVICE_OP_SYNC) {
        operand_size = 5u;
        instruction[0] = RUNA_OP_EXT;
        instruction[1] = operand_size;
        runa_write_u16_le(instruction + 2u, RUNA_BLOCK_DEVICE_MODULE_ID);
        instruction[4] = operation;
        runa_write_u16_le(instruction + 5u, resource_id);
        cursor = 7u;
    } else {
        size_t payload_size = 10u + data_size;
        if (payload_size > UINT8_MAX - 3u) return 0u;
        operand_size = (uint8_t)(3u + payload_size);
        instruction[0] = RUNA_OP_EXT;
        instruction[1] = operand_size;
        runa_write_u16_le(instruction + 2u, RUNA_BLOCK_DEVICE_MODULE_ID);
        instruction[4] = operation;
        runa_write_u16_le(instruction + 5u, resource_id);
        runa_write_u32_le(instruction + 7u, offset);
        runa_write_u32_le(instruction + 11u, length);
        if (data_size != 0u && data != NULL) memcpy(instruction + 15u, data, data_size);
        cursor = 15u + data_size;
    }
    instruction_bytes = cursor;
    if (with_return != 0) {
        instruction[cursor] = RUNA_OP_RETURN;
        instruction[cursor + 1u] = 1u;
        instruction[cursor + 2u] = 0u;
        cursor += 3u;
        instruction_bytes += 3u;
    }
    total = RUNA_HEADER_SIZE + instruction_bytes;
    job[0] = (uint8_t)'J'; job[1] = (uint8_t)'E'; job[2] = (uint8_t)'X'; job[3] = (uint8_t)'E';
    job[4] = RUNA_PROTOCOL_VERSION; job[5] = RUNA_IR_VERSION_V2;
    runa_write_u16_le(job + 6u, RUNA_HEADER_SIZE);
    runa_write_u32_le(job + 8u, id);
    runa_write_u32_le(job + 12u, (uint32_t)total);
    runa_write_u32_le(job + 16u, (uint32_t)instruction_bytes);
    runa_write_u16_le(job + 20u, (uint16_t)(with_return != 0 ? 2u : 1u));
    runa_write_u32_le(job + 24u, RUNA_MAX_STEPS);
    runa_write_u32_le(job + 28u, RUNA_MAX_RUNTIME_US);
    runa_write_u16_le(job + 32u, RUNA_MAX_RESULT_BYTES);
    runa_write_u16_le(job + 34u, RUNA_MAX_EMITS);
    runa_write_u32_le(job + 36u, RUNA_MAX_EMIT_BYTES);
    return total;
}

static size_t make_adversarial_job(uint8_t *job, uint8_t operation, uint16_t resource_id) {
    static const uint8_t data[] = { 0x11u, 0x22u, 0x33u, 0x44u };
    uint32_t offset = operation == RUNA_BLOCK_DEVICE_OP_ERASE ? 16u : 4u;
    uint32_t length = operation == RUNA_BLOCK_DEVICE_OP_SYNC ? 0u :
                      (operation == RUNA_BLOCK_DEVICE_OP_ERASE ? 16u : 4u);
    size_t size = make_job(job, 0x70000000u, operation, resource_id, offset, length,
                           operation == RUNA_BLOCK_DEVICE_OP_WRITE ? data : NULL,
                           operation == RUNA_BLOCK_DEVICE_OP_WRITE ? sizeof data : 0u, 1);
    size_t invalid_offset = size - 3u;
    memmove(job + invalid_offset + 5u, job + invalid_offset, 3u);
    job[invalid_offset] = RUNA_OP_EXT; job[invalid_offset + 1u] = 3u;
    runa_write_u16_le(job + invalid_offset + 2u, RUNA_BLOCK_DEVICE_MODULE_ID);
    job[invalid_offset + 4u] = 99u;
    runa_write_u32_le(job + 12u, (uint32_t)(size + 5u));
    runa_write_u32_le(job + 16u, (uint32_t)(size + 5u - RUNA_HEADER_SIZE));
    runa_write_u16_le(job + 20u, 3u);
    return size + 5u;
}

static int run_case(uint8_t *job, size_t job_size, const runa_resource_table_t *resources,
                    const runa_module_registry_t *registry, runa_block_device_mock_t *mock,
                    fixture_t *fixture, runa_status_t expected, uint8_t expected_events) {
    runa_block_device_hal_t hal = runa_block_device_mock_hal(mock);
    runa_module_t module = runa_block_device_module(&hal);
    runa_module_registry_t local_registry;
    runa_platform_t platform = { NULL, time_us, NULL };
    runa_event_sink_t sink = { capture, fixture };
    runa_execution_summary_t result;
    (void)registry;
    runa_registry_init(&local_registry);
    if (runa_registry_add(&local_registry, &module) != RUNA_OK) return 0;
    result = runa_process(job, job_size, resources, &local_registry, &platform, &sink);
    return result.error == expected && fixture->count == expected_events;
}

int main(void) {
    static uint8_t storage[256];
    static uint8_t job[RUNA_MAX_JOB_BYTES];
    static const uint8_t write_data[] = { 0xdeu, 0xadu, 0xbeu, 0xefu };
    static const uint8_t expected_capabilities[] = {
        2u, 1u, 2u, 32u, 54u, 0u, 1u, 8u, 0u, 8u, 0u, 0u, 0u, 1u, 32u, 0u,
        128u, 0u, 0u, 0u, 16u, 39u, 0u, 0u, 64u, 75u, 76u, 0u, 0u, 2u, 0u, 0u,
        22u, 0u, 1u, 0u, 12u, 0u, 1u, 1u, 12u, 0u, 1u, 15u, 4u, 1u, 192u, 0u,
        192u, 0u, 0u, 16u, 48u, 0u
    };
    runa_block_device_resource_config_t configuration = {
        256u, 1u, 4u, 16u, 192u, 192u, 128u, 0xffu,
        RUNA_BLOCK_CAP_READ | RUNA_BLOCK_CAP_WRITE | RUNA_BLOCK_CAP_ERASE |
            RUNA_BLOCK_CAP_SYNC | RUNA_BLOCK_CAP_PERSISTENT,
        4u, 0u
    };
    runa_resource_t resource = { 7u, RUNA_BLOCK_DEVICE_MODULE_ID,
                                 RUNA_BLOCK_DEVICE_RESOURCE_TYPE, 0u,
                                 RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE,
                                 (uintptr_t)0xbeefu, &configuration };
    runa_resource_table_t resources = { &resource, 1u };
    runa_block_device_mock_t mock;
    runa_block_device_hal_t hal;
    runa_module_t module;
    runa_module_registry_t registry;
    runa_capabilities_view_t capabilities;
    uint8_t encoded[128];
    size_t encoded_size = 0u;
    fixture_t fixture;
    size_t job_size;
    uint32_t index;

    runa_block_device_mock_init(&mock, storage, sizeof storage, sizeof storage, 0xffu);
    hal = runa_block_device_mock_hal(&mock);
    module = runa_block_device_module(&hal);
    runa_registry_init(&registry);
    if (!check(runa_registry_add(&registry, &module) == RUNA_OK, "registry add")) return 1;
    if (!check(runa_resource_table_validate(&resources, &registry) == RUNA_OK,
               "valid resource")) return 1;
    if (!check(runa_capabilities_encode(&registry, encoded, sizeof encoded, &encoded_size) == RUNA_OK &&
               encoded_size == sizeof expected_capabilities &&
               memcmp(encoded, expected_capabilities, sizeof expected_capabilities) == 0 &&
               runa_capabilities_decode(encoded, encoded_size, &capabilities) == RUNA_OK &&
               capabilities.module_count == 1u && capabilities.modules[0].module_id == 12u,
               "capability golden vector")) return 1;

    job_size = make_job(job, 0x10203040u, RUNA_BLOCK_DEVICE_OP_WRITE, 7u, 4u, 4u,
                        write_data, sizeof write_data, 1);
    memset(&fixture, 0, sizeof fixture);
    if (!check(run_case(job, job_size, &resources, &registry, &mock, &fixture, RUNA_OK, 2u) &&
               mock.write_calls == 1u && mock.last_handle == (uintptr_t)0xbeefu &&
               mock.last_offset == 4u && mock.last_size == 4u &&
               memcmp(storage + 4u, write_data, sizeof write_data) == 0,
               "write and exact stored bytes")) return 1;

    job_size = make_job(job, 0x10203041u, RUNA_BLOCK_DEVICE_OP_READ, 7u, 4u, 4u,
                        NULL, 0u, 1);
    memset(&fixture, 0, sizeof fixture);
    if (!check(run_case(job, job_size, &resources, &registry, &mock, &fixture, RUNA_OK, 3u) &&
               fixture.events[1][0] == RUNA_EVENT_MODULE_DATA && fixture.sizes[1] == 20u &&
               memcmp(fixture.events[1] + 16u, write_data, sizeof write_data) == 0,
               "persistent read exact bytes")) return 1;

    for (index = 0u; index < 64u; ++index) storage[index] = (uint8_t)index;
    job_size = make_job(job, 0x10203042u, RUNA_BLOCK_DEVICE_OP_READ, 7u, 0u, 64u,
                        NULL, 0u, 1);
    memset(&fixture, 0, sizeof fixture);
    if (!check(run_case(job, job_size, &resources, &registry, &mock, &fixture, RUNA_OK, 4u) &&
               fixture.sizes[1] == 64u && fixture.sizes[2] == 32u &&
               fixture.events[1][14] == 48u && fixture.events[1][16] == 0u &&
               fixture.events[1][63] == 47u && fixture.events[2][16] == 48u &&
               fixture.events[2][31] == 63u,
               "read chunking and exact bytes")) return 1;

    job_size = make_job(job, 0x10203043u, RUNA_BLOCK_DEVICE_OP_ERASE, 7u, 16u, 16u,
                        NULL, 0u, 1);
    memset(&fixture, 0, sizeof fixture);
    if (!check(run_case(job, job_size, &resources, &registry, &mock, &fixture, RUNA_OK, 2u) &&
               mock.erase_calls == 1u, "erase supported")) return 1;
    job_size = make_job(job, 0x10203044u, RUNA_BLOCK_DEVICE_OP_READ, 7u, 16u, 16u,
                        NULL, 0u, 1);
    memset(&fixture, 0, sizeof fixture);
    if (!check(run_case(job, job_size, &resources, &registry, &mock, &fixture, RUNA_OK, 3u) &&
               fixture.events[1][16] == 0xffu && fixture.events[1][31] == 0xffu &&
               storage[32] == 32u, "erase persistence and bounded region")) return 1;

    job_size = make_job(job, 0x10203045u, RUNA_BLOCK_DEVICE_OP_SYNC, 7u, 0u, 0u,
                        NULL, 0u, 1);
    memset(&fixture, 0, sizeof fixture);
    if (!check(run_case(job, job_size, &resources, &registry, &mock, &fixture, RUNA_OK, 2u) &&
               mock.sync_calls == 1u, "sync supported")) return 1;

    configuration.capacity_bytes = UINT32_MAX;
    mock.read_calls = 0u;
    job_size = make_job(job, 0x10203046u, RUNA_BLOCK_DEVICE_OP_READ, 7u, UINT32_MAX, 1u,
                        NULL, 0u, 1);
    memset(&fixture, 0, sizeof fixture);
    if (!check(run_case(job, job_size, &resources, &registry, &mock, &fixture,
                        RUNA_ERR_OUT_OF_RANGE, 1u) && mock.read_calls == 0u,
               "UINT32_MAX offset rejected")) return 1;
    job_size = make_job(job, 0x10203047u, RUNA_BLOCK_DEVICE_OP_READ, 7u,
                        UINT32_MAX - 10u, 100u, NULL, 0u, 1);
    mock.read_calls = 0u;
    memset(&fixture, 0, sizeof fixture);
    if (!check(run_case(job, job_size, &resources, &registry, &mock, &fixture,
                        RUNA_ERR_OUT_OF_RANGE, 1u) && mock.read_calls == 0u,
               "offset plus length overflow rejected")) return 1;
    configuration.capacity_bytes = 256u;

    resource.permissions = RUNA_PERMISSION_READ;
    job_size = make_job(job, 0x10203048u, RUNA_BLOCK_DEVICE_OP_WRITE, 7u, 4u, 4u,
                        write_data, sizeof write_data, 1);
    memset(&fixture, 0, sizeof fixture);
    if (!check(run_case(job, job_size, &resources, &registry, &mock, &fixture,
                        RUNA_ERR_ACCESS_DENIED, 1u) && mock.write_calls == 1u,
               "write permission rejected before HAL")) return 1;
    resource.permissions = RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE;

    configuration.capability_flags = RUNA_BLOCK_CAP_READ | RUNA_BLOCK_CAP_WRITE |
                                     RUNA_BLOCK_CAP_SYNC;
    job_size = make_job(job, 0x10203049u, RUNA_BLOCK_DEVICE_OP_ERASE, 7u, 16u, 16u,
                        NULL, 0u, 1);
    memset(&fixture, 0, sizeof fixture);
    if (!check(run_case(job, job_size, &resources, &registry, &mock, &fixture,
                        RUNA_ERR_UNSUPPORTED_MODULE, 1u) && mock.erase_calls == 1u,
               "unsupported erase rejected before HAL")) return 1;
    configuration.capability_flags |= RUNA_BLOCK_CAP_ERASE;

    mock.write_failure = RUNA_ERR_INTERNAL;
    mock.partial_write_bytes = 2u;
    job_size = make_job(job, 0x1020304au, RUNA_BLOCK_DEVICE_OP_WRITE, 7u, 8u, 4u,
                        write_data, sizeof write_data, 1);
    memset(&fixture, 0, sizeof fixture);
    if (!check(run_case(job, job_size, &resources, &registry, &mock, &fixture,
                        RUNA_ERR_INTERNAL, 2u) && storage[8] == 0xdeu &&
               storage[9] == 0xadu && storage[10] != 0xbeu,
               "partial write failure has no rollback")) return 1;
    mock.write_failure = RUNA_OK;

    configuration.capability_flags |= RUNA_BLOCK_CAP_ERASE | RUNA_BLOCK_CAP_SYNC;
    for (unsigned case_index = 0u; case_index < 1000u; ++case_index) {
        uint8_t operation = (uint8_t)(RUNA_BLOCK_DEVICE_OP_WRITE + (case_index % 3u));
        memset(&mock, 0, sizeof mock); memset(&fixture, 0, sizeof fixture);
        job_size = make_adversarial_job(job, operation, 7u);
        if (!check(run_case(job, job_size, &resources, &registry, &mock, &fixture,
                            RUNA_ERR_INVALID_OPCODE, 1u) && mock.write_calls == 0u &&
                   mock.erase_calls == 0u && mock.sync_calls == 0u,
                   "late invalid operation has no mutation")) {
            return 2;
        }
    }

    printf("Runa.BlockDevice native checks passed\n");
    return 0;
}
