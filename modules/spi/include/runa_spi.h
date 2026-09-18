#ifndef RUNA_SPI_H
#define RUNA_SPI_H

#include "runa_module.h"

#define RUNA_SPI_MODULE_ID 4u
#define RUNA_SPI_RESOURCE_TYPE 1u
#define RUNA_SPI_OP_TRANSFER 1u
#define RUNA_SPI_MAX_TRANSFER_BYTES 246u
#define RUNA_SPI_MAX_TIMEOUT_MS 5000u

enum { RUNA_SPI_ERR_IO = 25u };
enum {
    RUNA_SPI_CS_AUTOMATIC = 0u,
    RUNA_SPI_CS_SOFTWARE = 1u
};

typedef struct runa_spi_resource_config {
    uint8_t mode;
    uint8_t chip_select_behavior;
    uint16_t maximum_transfer_bytes;
    uint32_t clock_hz;
    uint32_t minimum_clock_hz;
    uint32_t maximum_clock_hz;
    uint16_t maximum_timeout_ms;
    uint16_t reserved;
    uintptr_t chip_select_handle;
} runa_spi_resource_config_t;

typedef struct runa_spi_hal {
    void *context;
    runa_status_t (*transfer)(void *context, uintptr_t device_handle,
                              const runa_spi_resource_config_t *configuration,
                              const uint8_t *transmit, size_t transmit_size,
                              uint8_t *receive, size_t receive_size,
                              uint16_t timeout_ms);
} runa_spi_hal_t;

runa_module_t runa_spi_module(runa_spi_hal_t *hal);

#endif
