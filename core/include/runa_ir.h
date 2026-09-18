#ifndef RUNA_IR_H
#define RUNA_IR_H

#include <stddef.h>
#include <stdint.h>
#include "runa_error.h"
#include "runa_limits.h"
#include "runa_opcode.h"

#define RUNA_IR_VERSION_V1 1u
#define RUNA_IR_VERSION_V2 2u

/* In IR V2, an EXT instruction is [0xe0, operand_size, module_id_le16,
 * operation_u8, module_payload...]. operand_size includes the three-byte
 * module/operation prefix. V1 peripheral opcodes are routed through the
 * compile-time registry's legacy-operation map. */

typedef struct runa_header {
    uint32_t job_id;
    uint32_t total_size;
    uint32_t instruction_bytes;
    uint16_t instruction_count;
    uint32_t max_steps;
    uint32_t max_runtime_us;
    uint16_t max_result_bytes;
    uint16_t max_emits;
    uint32_t max_emit_bytes;
} runa_header_t;

typedef struct runa_instruction {
    uint8_t opcode;
    uint8_t operand_size;
    const uint8_t *operands;
    uint32_t offset;
} runa_instruction_t;

typedef struct runa_decoded_job {
    runa_header_t header;
    uint8_t ir_version;
} runa_decoded_job_t;

uint16_t runa_read_u16_le(const uint8_t *data);
uint32_t runa_read_u32_le(const uint8_t *data);
void runa_write_u16_le(uint8_t *data, uint16_t value);
void runa_write_u32_le(uint8_t *data, uint32_t value);
runa_status_t runa_decode_header(const uint8_t *data, size_t size, runa_decoded_job_t *decoded);
runa_status_t runa_decode_instruction(const uint8_t *data, size_t size, uint32_t offset,
                                      runa_instruction_t *instruction);

#endif
