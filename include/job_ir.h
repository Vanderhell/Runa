#ifndef JOB_IR_H
#define JOB_IR_H

#include <stddef.h>
#include <stdint.h>

#define JOB_HEADER_SIZE 40u
#define JOB_MAGIC_0 ((uint8_t)'J')
#define JOB_MAGIC_1 ((uint8_t)'E')
#define JOB_MAGIC_2 ((uint8_t)'X')
#define JOB_MAGIC_3 ((uint8_t)'E')
#define JOB_PROTOCOL_VERSION 1u
#define JOB_IR_VERSION 1u
#define JOB_REGISTER_COUNT 8u
#define JOB_MAX_BYTES 2048u
#define JOB_MAX_INSTRUCTIONS 256u
#define JOB_MAX_STEPS 10000u
#define JOB_MAX_RUNTIME_US 5000000u
#define JOB_MAX_RESULT_BYTES 32u
#define JOB_MAX_EMITS 128u
#define JOB_MAX_EMIT_BYTES 512u
#define JOB_MAX_SINGLE_DELAY_MS 1000u
#define JOB_MAX_RESOURCES 64u
#define JOB_MAX_EVENT_BYTES 64u

typedef enum job_opcode {
    JOB_OP_NOP = 0x00,
    JOB_OP_LOAD_CONST = 0x01,
    JOB_OP_MOV = 0x02,
    JOB_OP_ADD = 0x03,
    JOB_OP_SUB = 0x04,
    JOB_OP_AND = 0x05,
    JOB_OP_OR = 0x06,
    JOB_OP_XOR = 0x07,
    JOB_OP_NOT = 0x08,
    JOB_OP_SHL = 0x09,
    JOB_OP_SHR = 0x0a,
    JOB_OP_CMP_EQ = 0x10,
    JOB_OP_CMP_NE = 0x11,
    JOB_OP_CMP_LT = 0x12,
    JOB_OP_CMP_LE = 0x13,
    JOB_OP_CMP_GT = 0x14,
    JOB_OP_CMP_GE = 0x15,
    JOB_OP_JUMP = 0x20,
    JOB_OP_JUMP_IF = 0x21,
    JOB_OP_JUMP_IF_NOT = 0x22,
    JOB_OP_GPIO_READ = 0x30,
    JOB_OP_GPIO_WRITE = 0x31,
    JOB_OP_ADC_READ = 0x32,
    JOB_OP_PWM_WRITE = 0x33,
    JOB_OP_DELAY_MS = 0x40,
    JOB_OP_EMIT = 0x50,
    JOB_OP_RETURN = 0x51
} job_opcode_t;

typedef struct job_header {
    uint32_t job_id;
    uint32_t total_size;
    uint32_t instruction_bytes;
    uint16_t instruction_count;
    uint32_t max_steps;
    uint32_t max_runtime_us;
    uint16_t max_result_bytes;
    uint16_t max_emits;
    uint32_t max_emit_bytes;
} job_header_t;

typedef struct job_instruction {
    uint8_t opcode;
    uint8_t operand_size;
    const uint8_t *operands;
    uint32_t offset;
} job_instruction_t;

uint16_t job_read_u16_le(const uint8_t *data);
uint32_t job_read_u32_le(const uint8_t *data);
void job_write_u16_le(uint8_t *data, uint16_t value);
void job_write_u32_le(uint8_t *data, uint32_t value);

#endif

