#include "runa_dac.h"
#include "runa_limits.h"
#include "runa_registry.h"
#include "runa_resource.h"
#include "runa_validator.h"
#include <stdio.h>
static runa_status_t noop(void *context, uintptr_t handle, uint32_t value) { (void)context; (void)handle; (void)value; return RUNA_OK; }
int main(int argc, char **argv) {
    uint8_t data[RUNA_MAX_JOB_BYTES]; FILE *file; size_t size; runa_decoded_job_t decoded; runa_validation_error_t error;
    runa_dac_resource_config_t config = {4095u, 12u, {0u, 0u, 0u}}; runa_dac_hal_t hal = {NULL, noop}; runa_module_t module = runa_dac_module(&hal);
    runa_module_registry_t registry; runa_resource_t item = {9u, 9u, 1u, 0u, RUNA_PERMISSION_WRITE, 0u, &config}; runa_resource_table_t resources = {&item, 1u};
    if (argc != 2) return 0;
#ifdef _MSC_VER
    if (fopen_s(&file, argv[1], "rb") != 0) return 2;
#else
    file = fopen(argv[1], "rb"); if (file == NULL) return 2;
#endif
    size = fread(data, 1u, sizeof data, file); fclose(file); runa_registry_init(&registry); (void)runa_registry_add(&registry, &module);
    (void)runa_validate(data, size, &registry, &resources, &decoded, &error); return 0;
}
