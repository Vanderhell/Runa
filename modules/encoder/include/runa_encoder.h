#ifndef RUNA_ENCODER_H
#define RUNA_ENCODER_H

#include "runa_module.h"

#define RUNA_ENCODER_MODULE_ID 10u
#define RUNA_ENCODER_RESOURCE_TYPE 1u
#define RUNA_ENCODER_OP_READ 1u
#define RUNA_ENCODER_OP_RESET 2u
#define RUNA_ENCODER_OP_READ_RESET 3u

#define RUNA_ENCODER_DECODE_X1 1u
#define RUNA_ENCODER_DECODE_X2 2u
#define RUNA_ENCODER_DECODE_X4 4u

enum {
    RUNA_ENCODER_ERR_IO = 32u,
    RUNA_ENCODER_ERR_OVERFLOW = 33u,
    RUNA_ENCODER_ERR_UNSUPPORTED = 34u
};

/* Position is exposed as a signed int32_t. When returned through a Runa
 * register, the exact two's-complement uint32_t bit pattern is used. */
typedef struct runa_encoder_resource_config {
    int16_t a_pin;
    int16_t b_pin;
    uint8_t decode_mode;
    uint8_t invert_direction;
    int32_t reset_value;
    uint32_t reserved;
} runa_encoder_resource_config_t;

typedef struct runa_encoder_hal {
    void *context;
    runa_status_t (*read)(void *context, uintptr_t handle, int32_t *position);
    runa_status_t (*reset)(void *context, uintptr_t handle);
    /* Implementations must make the read and reset indivisible to Runa. */
    runa_status_t (*read_reset)(void *context, uintptr_t handle, int32_t *position);
} runa_encoder_hal_t;

runa_module_t runa_encoder_module(runa_encoder_hal_t *hal);

#endif
