#ifndef RUNA_VALIDATOR_H
#define RUNA_VALIDATOR_H

#include "runa_ir.h"
#include "runa_registry.h"

typedef struct runa_validation_error {
    runa_status_t code;
    uint16_t instruction_index;
    uint32_t detail;
} runa_validation_error_t;

runa_status_t runa_validate(const uint8_t *data, size_t size,
                            const runa_module_registry_t *registry,
                            const runa_resource_table_t *resources,
                            runa_decoded_job_t *decoded,
                            runa_validation_error_t *error);

#endif
