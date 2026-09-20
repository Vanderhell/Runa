#include "runa_block_device.h"
#include "runa_block_device_mock_hal.h"
#include "runa_limits.h"
#include "runa_registry.h"
#include "runa_runtime.h"

#include <stdio.h>

static int discard(void *context, const uint8_t *data, size_t size) {
    (void)context;
    (void)data;
    (void)size;
    return 0;
}

static uint64_t time_us(void *context) {
    (void)context;
    return 0u;
}

int main(void) {
    uint8_t input[RUNA_MAX_JOB_BYTES];
    uint8_t storage[256];
    size_t size = fread(input, 1u, sizeof input, stdin);
    runa_block_device_mock_t mock;
    runa_block_device_hal_t hal;
    runa_module_t module;
    runa_module_registry_t registry;
    runa_block_device_resource_config_t config = {
        256u, 1u, 1u, 1u, 192u, 192u, 192u, 0xffu,
        RUNA_BLOCK_CAP_READ | RUNA_BLOCK_CAP_WRITE | RUNA_BLOCK_CAP_ERASE |
            RUNA_BLOCK_CAP_SYNC,
        0u, 0u
    };
    runa_resource_t resource = {
        12u, RUNA_BLOCK_DEVICE_MODULE_ID, RUNA_BLOCK_DEVICE_RESOURCE_TYPE, 0u,
        RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE, 1u, &config
    };
    runa_resource_table_t resources = { &resource, 1u };
    runa_platform_t platform = { NULL, time_us, NULL };
    runa_event_sink_t sink = { discard, NULL };

    if (size == 0u) return 0;
    runa_block_device_mock_init(&mock, storage, sizeof storage, sizeof storage, 0xffu);
    hal = runa_block_device_mock_hal(&mock);
    module = runa_block_device_module(&hal);
    runa_registry_init(&registry);
    if (runa_registry_add(&registry, &module) == RUNA_OK)
        (void)runa_process(input, size, &resources, &registry, &platform, &sink);
    return 0;
}
