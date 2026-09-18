#ifndef RUNA_CAPABILITIES_H
#define RUNA_CAPABILITIES_H

#include <stddef.h>
#include <stdint.h>
#include "runa_limits.h"
#include "runa_registry.h"

#define RUNA_CAPABILITIES_FORMAT_VERSION 2u
#define RUNA_CAPABILITIES_HEADER_SIZE 32u
#define RUNA_MAX_CAPABILITY_BYTES RUNA_MAX_JOB_BYTES
#define RUNA_MAX_MODULE_CAPABILITY_PAYLOAD 64u
#define RUNA_CAPABILITY_RECORD_MODULE 1u

/* Capabilities V2 uses a 32-byte fixed header followed by bounded records.
 * Each record starts with uint16 length and uint8 type; unknown record types
 * can be skipped using their length. Module payload bytes are module-owned. */

typedef struct runa_module_capability_view {
    uint16_t module_id;
    uint8_t abi_version;
    uint8_t module_version;
    uint16_t payload_size;
    const uint8_t *payload;
} runa_module_capability_view_t;

typedef struct runa_capabilities_view {
    uint8_t capability_version;
    uint8_t protocol_version;
    uint8_t ir_version;
    uint8_t register_count;
    uint32_t max_job_bytes;
    uint16_t max_instructions;
    uint16_t max_result_bytes;
    uint16_t max_emits;
    uint32_t max_steps;
    uint32_t max_runtime_us;
    uint32_t max_emit_bytes;
    uint8_t module_record_count;
    uint8_t module_count;
    runa_module_capability_view_t modules[RUNA_MAX_MODULES];
} runa_capabilities_view_t;

runa_status_t runa_capabilities_encode(const runa_module_registry_t *registry,
                                       uint8_t *output, size_t capacity, size_t *written);
runa_status_t runa_capabilities_decode(const uint8_t *data, size_t size,
                                       runa_capabilities_view_t *view);

#endif
