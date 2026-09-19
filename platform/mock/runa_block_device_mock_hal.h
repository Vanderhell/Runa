#ifndef RUNA_BLOCK_DEVICE_MOCK_HAL_H
#define RUNA_BLOCK_DEVICE_MOCK_HAL_H

#include "runa_block_device.h"

typedef struct runa_block_device_mock {
    uint8_t *storage;
    size_t storage_capacity;
    size_t configured_capacity;
    uint8_t fill_value;
    runa_status_t read_failure;
    runa_status_t write_failure;
    runa_status_t erase_failure;
    runa_status_t sync_failure;
    size_t partial_write_bytes;
    size_t partial_erase_bytes;
    uint32_t read_calls;
    uint32_t write_calls;
    uint32_t erase_calls;
    uint32_t sync_calls;
    uintptr_t last_handle;
    uint32_t last_offset;
    size_t last_size;
} runa_block_device_mock_t;

void runa_block_device_mock_init(runa_block_device_mock_t *mock, uint8_t *storage,
                                 size_t storage_capacity, size_t configured_capacity,
                                 uint8_t fill_value);
runa_block_device_hal_t runa_block_device_mock_hal(runa_block_device_mock_t *mock);

#endif
