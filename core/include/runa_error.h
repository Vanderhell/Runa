#ifndef RUNA_ERROR_H
#define RUNA_ERROR_H

#include <stdint.h>

typedef uint8_t runa_status_t;

enum {
    RUNA_OK = 0,
    RUNA_ERR_INVALID_FORMAT = 1,
    RUNA_ERR_BAD_MAGIC = 2,
    RUNA_ERR_UNSUPPORTED_PROTOCOL = 3,
    RUNA_ERR_UNSUPPORTED_IR_VERSION = 4,
    RUNA_ERR_JOB_TOO_LARGE = 5,
    RUNA_ERR_INVALID_OPCODE = 6,
    RUNA_ERR_INVALID_OPERAND = 7,
    RUNA_ERR_INVALID_REGISTER = 8,
    RUNA_ERR_INVALID_JUMP = 9,
    RUNA_ERR_INVALID_RESOURCE = 10,
    RUNA_ERR_RESOURCE_TYPE = 11,
    RUNA_ERR_ACCESS_DENIED = 12,
    RUNA_ERR_OUT_OF_RANGE = 13,
    RUNA_ERR_STEP_LIMIT = 14,
    RUNA_ERR_RUNTIME_LIMIT = 15,
    RUNA_ERR_RESULT_LIMIT = 16,
    RUNA_ERR_EMIT_LIMIT = 17,
    /* Values at and above MODULE_BASE are module-local status domains. The
     * module ID and operation identify their meaning; do not compare these
     * values globally across modules. */
    RUNA_ERR_MODULE_BASE = 18,
    RUNA_ERR_IO_TIMEOUT = 21,
    RUNA_ERR_CANCELLED = 22,
    RUNA_ERR_INTERNAL = 23,
    RUNA_ERR_UNSUPPORTED_MODULE = 24
};

#endif
