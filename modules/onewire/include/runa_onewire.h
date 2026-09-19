#ifndef RUNA_ONEWIRE_H
#define RUNA_ONEWIRE_H

#include "runa_module.h"

#define RUNA_ONEWIRE_MODULE_ID 11u

#define RUNA_ONEWIRE_BUS_RESOURCE_TYPE 1u
#define RUNA_ONEWIRE_DEVICE_RESOURCE_TYPE 2u

#define RUNA_ONEWIRE_OP_RESET 1u
#define RUNA_ONEWIRE_OP_TRANSFER 2u
#define RUNA_ONEWIRE_OP_ROM_SEARCH 3u

#define RUNA_ONEWIRE_MAX_TX_BYTES 240u
#define RUNA_ONEWIRE_MAX_RX_BYTES 240u
#define RUNA_ONEWIRE_MAX_SEARCH_RESULTS 16u
#define RUNA_ONEWIRE_MAX_TIMEOUT_US 5000000u
#define RUNA_ONEWIRE_MAX_STRONG_PULLUP_US 5000000u

#define RUNA_ONEWIRE_TRANSFER_SKIP_ROM 0x01u
#define RUNA_ONEWIRE_TRANSFER_STRONG_PULLUP 0x02u
#define RUNA_ONEWIRE_TRANSFER_FLAGS_MASK \
    (RUNA_ONEWIRE_TRANSFER_SKIP_ROM | RUNA_ONEWIRE_TRANSFER_STRONG_PULLUP)

#define RUNA_ONEWIRE_CAPABILITY_VERSION 1u
#define RUNA_ONEWIRE_CAPABILITY_SIZE 16u

enum {
    RUNA_ONEWIRE_ERR_IO = 25u,
    RUNA_ONEWIRE_ERR_CRC = 26u,
    RUNA_ONEWIRE_ERR_SEARCH = 27u,
    RUNA_ONEWIRE_ERR_STRONG_PULLUP = 28u,
    RUNA_ONEWIRE_ERR_NO_PRESENCE = 29u
};

enum {
    RUNA_ONEWIRE_CAP_STRONG_PULLUP = 0x01u,
    RUNA_ONEWIRE_CAP_BROADCAST = 0x02u,
    RUNA_ONEWIRE_CAP_DEVICE_RESOURCE = 0x04u,
    RUNA_ONEWIRE_CAP_ROM_CRC = 0x08u
};

typedef struct runa_onewire_bus_resource_config {
    uint16_t maximum_tx_bytes;
    uint16_t maximum_rx_bytes;
    uint16_t maximum_search_results;
    uint8_t strong_pullup_supported;
    uint8_t broadcast_supported;
    uint8_t validate_rom_crc;
    uint8_t reserved;
    uint32_t maximum_timeout_us;
} runa_onewire_bus_resource_config_t;

typedef struct runa_onewire_device_resource_config {
    uint16_t bus_resource_id;
    uint16_t reserved;
    uint8_t rom_id[8];
} runa_onewire_device_resource_config_t;

typedef struct runa_onewire_hal {
    void *context;
    uint8_t strong_pullup_supported;
    uint8_t reserved[3];
    runa_status_t (*reset)(void *context, uintptr_t bus_handle, uint32_t timeout_us,
                           uint8_t *presence);
    runa_status_t (*transfer)(void *context, uintptr_t bus_handle, const uint8_t *rom_id,
                              const uint8_t *transmit, size_t transmit_size,
                              uint8_t *receive, size_t receive_size, uint32_t timeout_us,
                              uint32_t strong_pullup_us);
    runa_status_t (*search)(void *context, uintptr_t bus_handle, uint8_t *rom_ids,
                            size_t rom_ids_capacity, size_t maximum_results,
                            size_t *result_count, uint32_t timeout_us);
} runa_onewire_hal_t;

uint8_t runa_onewire_rom_crc8(const uint8_t *rom_id);
int runa_onewire_rom_crc_valid(const uint8_t *rom_id);
runa_module_t runa_onewire_module(runa_onewire_hal_t *hal);

#endif
