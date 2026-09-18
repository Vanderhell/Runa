#ifndef RUNA_OPCODE_H
#define RUNA_OPCODE_H

typedef enum runa_opcode {
    RUNA_OP_NOP = 0x00,
    RUNA_OP_LOAD_CONST = 0x01,
    RUNA_OP_MOV = 0x02,
    RUNA_OP_ADD = 0x03,
    RUNA_OP_SUB = 0x04,
    RUNA_OP_AND = 0x05,
    RUNA_OP_OR = 0x06,
    RUNA_OP_XOR = 0x07,
    RUNA_OP_NOT = 0x08,
    RUNA_OP_SHL = 0x09,
    RUNA_OP_SHR = 0x0a,
    RUNA_OP_CMP_EQ = 0x10,
    RUNA_OP_CMP_NE = 0x11,
    RUNA_OP_CMP_LT = 0x12,
    RUNA_OP_CMP_LE = 0x13,
    RUNA_OP_CMP_GT = 0x14,
    RUNA_OP_CMP_GE = 0x15,
    RUNA_OP_JUMP = 0x20,
    RUNA_OP_JUMP_IF = 0x21,
    RUNA_OP_JUMP_IF_NOT = 0x22,
    RUNA_OP_DELAY_MS = 0x40,
    RUNA_OP_EMIT = 0x50,
    RUNA_OP_RETURN = 0x51,
    RUNA_OP_EXT = 0xe0
} runa_opcode_t;

#endif
