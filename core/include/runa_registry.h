#ifndef RUNA_REGISTRY_H
#define RUNA_REGISTRY_H

#include <stdint.h>
#include "runa_error.h"
#include "runa_module.h"

#define RUNA_MAX_MODULES 16u

typedef struct runa_module_registry {
    const runa_module_t *modules[RUNA_MAX_MODULES];
    uint8_t count;
} runa_module_registry_t;

void runa_registry_init(runa_module_registry_t *registry);
runa_status_t runa_registry_add(runa_module_registry_t *registry, const runa_module_t *module);
runa_status_t runa_registry_validate(const runa_module_registry_t *registry);
const runa_module_t *runa_registry_find(const runa_module_registry_t *registry, uint16_t module_id);
runa_status_t runa_registry_find_legacy(const runa_module_registry_t *registry, uint8_t opcode,
                                        const runa_module_t **module, uint8_t *operation);
runa_status_t runa_registry_validate_instruction(const runa_module_registry_t *registry,
                                                  const runa_module_job_t *job,
                                                  const runa_module_instruction_t *instruction,
                                                  uint32_t *detail);
runa_status_t runa_registry_begin_job(const runa_module_registry_t *registry,
                                      const runa_module_job_t *job);
runa_status_t runa_registry_end_job(const runa_module_registry_t *registry,
                                    const runa_module_job_t *job,
                                    runa_status_t execution_status);

#endif
