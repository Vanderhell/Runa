#ifndef RUNA_I2C_H
#define RUNA_I2C_H

#include "runa_module.h"

#define RUNA_I2C_MODULE_ID 5u
#define RUNA_I2C_RESOURCE_TYPE 1u
#define RUNA_I2C_OP_TRANSFER 1u
#define RUNA_I2C_MAX_TX_BYTES 246u
#define RUNA_I2C_MAX_RX_BYTES 246u
#define RUNA_I2C_MAX_TIMEOUT_MS 5000u
#define RUNA_I2C_MIN_CLOCK_HZ 10000u
#define RUNA_I2C_MAX_CLOCK_HZ 1000000u

enum { RUNA_I2C_ERR_NACK = 25u, RUNA_I2C_ERR_BUS = 26u };

typedef struct runa_i2c_resource_config {
    uint8_t bus;
    uint8_t address;
    uint16_t maximum_tx_bytes;
    uint16_t maximum_rx_bytes;
    uint32_t clock_hz;
    uint16_t maximum_timeout_ms;
    uint16_t reserved;
} runa_i2c_resource_config_t;

typedef struct runa_i2c_hal {
    void *context;
    runa_status_t (*transfer)(void *context, uintptr_t device_handle,
                              const runa_i2c_resource_config_t *configuration,
                              const uint8_t *transmit, size_t transmit_size,
                              uint8_t *receive, size_t receive_size,
                              uint16_t timeout_ms);
} runa_i2c_hal_t;

runa_module_t runa_i2c_module(runa_i2c_hal_t *hal);

#endif
