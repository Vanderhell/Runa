#include "runa_watchdog.h"

#include "runa_ir.h"
#include "runa_platform.h"
#include "runa_registry.h"
#include "runa_resource.h"
#include "runa_result.h"
#include "runa_runtime.h"

#include <stdio.h>

static runa_status_t status(void *context, uintptr_t handle, runa_watchdog_status_t *output) {
    (void)context; (void)handle;
    output->flags = 0u; output->reserved[0] = 0u; output->reserved[1] = 0u;
    output->reserved[2] = 0u; output->timeout_ms = 0u;
    return RUNA_OK;
}
static runa_status_t arm(void *context, uintptr_t handle, uint32_t timeout_ms) {
    (void)context; (void)handle; (void)timeout_ms; return RUNA_OK;
}
static runa_status_t feed(void *context, uintptr_t handle) {
    (void)context; (void)handle; return RUNA_OK;
}
static runa_status_t disarm(void *context, uintptr_t handle) {
    (void)context; (void)handle; return RUNA_OK;
}
static uint64_t time_us(void *context) { (void)context; return 0u; }
static int sink(void *context, const uint8_t *data, size_t size) {
    (void)context; (void)data; (void)size; return 0;
}

int main(int argc, char **argv) {
    uint8_t data[RUNA_MAX_JOB_BYTES];
    size_t size;
    FILE *file;
    runa_watchdog_hal_t hal = { NULL, status, arm, feed, disarm };
    runa_module_t module = runa_watchdog_module(&hal);
    runa_module_registry_t registry;
    runa_watchdog_resource_config_t configuration = {
        1u, RUNA_WATCHDOG_MAX_TIMEOUT_MS, 0u, 0x0fu, 0u, 0u, 0u
    };
    runa_resource_t resource = { 14u, RUNA_WATCHDOG_MODULE_ID,
                                 RUNA_WATCHDOG_RESOURCE_TYPE, 0u,
                                 RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE, 0u,
                                 &configuration };
    runa_resource_table_t resources = { &resource, 1u };
    runa_platform_t platform = { NULL, time_us, NULL };
    runa_event_sink_t events = { sink, NULL };
    if (argc != 2) return 0;
#ifdef _MSC_VER
    if (fopen_s(&file, argv[1], "rb") != 0) return 2;
#else
    file = fopen(argv[1], "rb"); if (file == NULL) return 2;
#endif
    size = fread(data, 1u, sizeof data, file);
    fclose(file);
    runa_registry_init(&registry);
    (void)runa_registry_add(&registry, &module);
    (void)runa_process(data, size, &resources, &registry, &platform, &events);
    return 0;
}
