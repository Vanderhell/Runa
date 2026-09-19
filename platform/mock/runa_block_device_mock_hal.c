#include "runa_block_device_mock_hal.h"

#include <string.h>

static int mock_bounds(const runa_block_device_mock_t *mock, uint32_t offset, size_t size) {
    return mock != NULL && mock->storage != NULL && (size_t)offset <= mock->configured_capacity &&
           (size_t)offset <= mock->storage_capacity &&
           size <= mock->configured_capacity - (size_t)offset &&
           size <= mock->storage_capacity - (size_t)offset;
}

static runa_status_t mock_read(void *context, uintptr_t handle, uint32_t offset,
                               uint8_t *destination, size_t size) {
    runa_block_device_mock_t *mock = (runa_block_device_mock_t *)context;
    if (!mock_bounds(mock, offset, size) || destination == NULL) return RUNA_ERR_INTERNAL;
    mock->read_calls++;
    mock->last_handle = handle;
    mock->last_offset = offset;
    mock->last_size = size;
    if (mock->read_failure != RUNA_OK) return mock->read_failure;
    memcpy(destination, mock->storage + offset, size);
    return RUNA_OK;
}

static runa_status_t mock_write(void *context, uintptr_t handle, uint32_t offset,
                                const uint8_t *source, size_t size) {
    runa_block_device_mock_t *mock = (runa_block_device_mock_t *)context;
    size_t effect = size;
    if (!mock_bounds(mock, offset, size) || source == NULL) return RUNA_ERR_INTERNAL;
    mock->write_calls++;
    mock->last_handle = handle;
    mock->last_offset = offset;
    mock->last_size = size;
    if (mock->write_failure != RUNA_OK) {
        if (mock->partial_write_bytes < effect) effect = mock->partial_write_bytes;
        if (effect != 0u) memcpy(mock->storage + offset, source, effect);
        return mock->write_failure;
    }
    memcpy(mock->storage + offset, source, size);
    return RUNA_OK;
}

static runa_status_t mock_erase(void *context, uintptr_t handle, uint32_t offset,
                                size_t size) {
    runa_block_device_mock_t *mock = (runa_block_device_mock_t *)context;
    size_t effect = size;
    if (!mock_bounds(mock, offset, size)) return RUNA_ERR_INTERNAL;
    mock->erase_calls++;
    mock->last_handle = handle;
    mock->last_offset = offset;
    mock->last_size = size;
    if (mock->erase_failure != RUNA_OK) {
        if (mock->partial_erase_bytes < effect) effect = mock->partial_erase_bytes;
        if (effect != 0u) memset(mock->storage + offset, mock->fill_value, effect);
        return mock->erase_failure;
    }
    memset(mock->storage + offset, mock->fill_value, size);
    return RUNA_OK;
}

static runa_status_t mock_sync(void *context, uintptr_t handle) {
    runa_block_device_mock_t *mock = (runa_block_device_mock_t *)context;
    if (mock == NULL) return RUNA_ERR_INTERNAL;
    mock->sync_calls++;
    mock->last_handle = handle;
    mock->last_offset = 0u;
    mock->last_size = 0u;
    return mock->sync_failure;
}

void runa_block_device_mock_init(runa_block_device_mock_t *mock, uint8_t *storage,
                                 size_t storage_capacity, size_t configured_capacity,
                                 uint8_t fill_value) {
    if (mock == NULL) return;
    memset(mock, 0, sizeof *mock);
    mock->storage = storage;
    mock->storage_capacity = storage_capacity;
    mock->configured_capacity = configured_capacity;
    mock->fill_value = fill_value;
    mock->read_failure = RUNA_OK;
    mock->write_failure = RUNA_OK;
    mock->erase_failure = RUNA_OK;
    mock->sync_failure = RUNA_OK;
    if (storage != NULL && storage_capacity != 0u) memset(storage, fill_value, storage_capacity);
}

runa_block_device_hal_t runa_block_device_mock_hal(runa_block_device_mock_t *mock) {
    runa_block_device_hal_t hal = { mock, mock_read, mock_write, mock_erase, mock_sync };
    return hal;
}
