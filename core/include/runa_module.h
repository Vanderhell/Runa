#ifndef RUNA_MODULE_H
#define RUNA_MODULE_H

#include <stddef.h>
#include <stdint.h>
#include "runa_error.h"
#include "runa_platform.h"
#include "runa_resource.h"
#include "runa_opcode.h"

#define RUNA_MODULE_ABI_VERSION 1u

typedef runa_status_t (*runa_module_emit_data_fn)(void *context, uint16_t module_id,
                                                 uint16_t instruction_index, uint16_t sequence,
                                                 const uint8_t *data, size_t size);
/* Data frames have a 16-byte envelope and at most 48 module-owned bytes.
 * Modules split larger bounded transfers into sequenced frames. */

typedef struct runa_module_job {
    uint32_t job_id;
    uint16_t instruction_count;
    uint16_t reserved;
    uint32_t max_steps;
    uint32_t max_runtime_us;
    uint16_t max_result_bytes;
    uint16_t max_emits;
    uint32_t max_emit_bytes;
    uint32_t *registers;
    size_t register_count;
    const runa_resource_table_t *resources;
    const runa_platform_t *platform;
    void *user_data;
    runa_module_emit_data_fn emit_data;
    void *emit_context;
} runa_module_job_t;

typedef struct runa_module_instruction {
    uint16_t module_id;
    uint8_t operation;
    uint8_t operand_size;
    const uint8_t *operands;
    uint16_t instruction_index;
    uint32_t byte_offset;
} runa_module_instruction_t;

typedef runa_status_t (*runa_module_resource_validate_fn)(void *context,
                                                          const runa_resource_t *resource,
                                                          uint32_t *detail);

typedef runa_status_t (*runa_module_validate_fn)(void *context,
                                                  const runa_module_job_t *job,
                                                  const runa_module_instruction_t *instruction,
                                                  uint32_t *detail);
typedef runa_status_t (*runa_module_execute_fn)(void *context,
                                                 runa_module_job_t *job,
                                                 const runa_module_instruction_t *instruction,
                                                 uint32_t *detail);
typedef runa_status_t (*runa_module_job_begin_fn)(void *context, const runa_module_job_t *job);
typedef runa_status_t (*runa_module_job_end_fn)(void *context, const runa_module_job_t *job,
                                                 runa_status_t execution_status);
typedef size_t (*runa_module_capabilities_fn)(void *context, uint8_t *output, size_t capacity);

typedef struct runa_legacy_operation {
    uint8_t opcode;
    uint8_t operation;
} runa_legacy_operation_t;

typedef struct runa_module {
    uint16_t module_id;
    uint8_t abi_version;
    uint8_t module_version;
    runa_module_validate_fn validate;
    runa_module_execute_fn execute;
    runa_module_job_begin_fn begin;
    runa_module_job_end_fn end;
    runa_module_capabilities_fn capabilities;
    void *context;
    const runa_legacy_operation_t *legacy_operations;
    uint8_t legacy_operation_count;
    runa_module_resource_validate_fn validate_resource;
} runa_module_t;

/* Descriptors have static or composition-scope lifetime. Modules are validated
 * before any begin callback; end is called in reverse order after execution. */

#endif
