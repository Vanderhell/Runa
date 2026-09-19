#ifndef RUNA_BLOCK_DEVICE_H
#define RUNA_BLOCK_DEVICE_H

#include "runa_module.h"

#define RUNA_BLOCK_DEVICE_MODULE_ID 12u
#define RUNA_BLOCK_DEVICE_RESOURCE_TYPE 1u

enum {
    RUNA_BLOCK_DEVICE_OP_READ = 1u,
    RUNA_BLOCK_DEVICE_OP_WRITE = 2u,
    RUNA_BLOCK_DEVICE_OP_ERASE = 3u,
    RUNA_BLOCK_DEVICE_OP_SYNC = 4u
};

#define RUNA_BLOCK_MAX_READ_BYTES 192u
#define RUNA_BLOCK_MAX_WRITE_BYTES 192u
#define RUNA_BLOCK_MAX_ERASE_BYTES 4096u
#define RUNA_BLOCK_OFFSET_WIDTH_BYTES 4u
#define RUNA_BLOCK_CAPABILITY_SIZE 12u

enum {
    RUNA_BLOCK_CAP_READ = 1u,
    RUNA_BLOCK_CAP_WRITE = 2u,
    RUNA_BLOCK_CAP_ERASE = 4u,
    RUNA_BLOCK_CAP_SYNC = 8u,
    RUNA_BLOCK_CAP_ERASE_REQUIRED_BEFORE_WRITE = 16u,
    RUNA_BLOCK_CAP_PERSISTENT = 32u
};

typedef struct runa_block_device_resource_config {
    uint32_t capacity_bytes;
    uint32_t read_alignment;
    uint32_t write_alignment;
    uint32_t erase_alignment;
    uint16_t max_read_bytes;
    uint16_t max_write_bytes;
    uint16_t max_erase_bytes;
    uint8_t erased_value;
    uint8_t capability_flags;
    uint16_t atomic_write_size;
    uint16_t reserved;
} runa_block_device_resource_config_t;

typedef struct runa_block_device_hal {
    void *context;
    runa_status_t (*read)(void *context, uintptr_t handle, uint32_t offset,
                          uint8_t *destination, size_t size);
    runa_status_t (*write)(void *context, uintptr_t handle, uint32_t offset,
                           const uint8_t *source, size_t size);
    runa_status_t (*erase)(void *context, uintptr_t handle, uint32_t offset,
                           size_t size);
    runa_status_t (*sync)(void *context, uintptr_t handle);
} runa_block_device_hal_t;

runa_module_t runa_block_device_module(runa_block_device_hal_t *hal);

#endif
