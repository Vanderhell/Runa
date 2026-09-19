#ifndef RUNA_CAN_H
#define RUNA_CAN_H

#include "runa_module.h"

#define RUNA_CAN_MODULE_ID 7u
#define RUNA_CAN_RESOURCE_TYPE 1u
#define RUNA_CAN_OP_TRANSMIT 1u
#define RUNA_CAN_OP_RECEIVE 2u
#define RUNA_CAN_OP_REQUEST_RESPONSE 3u
#define RUNA_CAN_MAX_PAYLOAD_BYTES 8u
#define RUNA_CAN_MAX_TIMEOUT_US 5000000u
#define RUNA_CAN_MIN_BITRATE 10000u
#define RUNA_CAN_MAX_BITRATE 1000000u

enum { RUNA_CAN_FRAME_EXTENDED = 1u };
enum { RUNA_CAN_FILTER_STANDARD = 0u, RUNA_CAN_FILTER_EXTENDED = 1u };
enum { RUNA_CAN_ERR_IO = 29u, RUNA_CAN_ERR_CONTROLLER = 30u, RUNA_CAN_ERR_FILTER = 31u };

typedef struct runa_can_frame {
    uint32_t id;
    uint8_t flags;
    uint8_t length;
    uint8_t data[RUNA_CAN_MAX_PAYLOAD_BYTES];
} runa_can_frame_t;

typedef struct runa_can_resource_config {
    uint8_t controller;
    uint8_t filter_mode;
    uint8_t listen_only;
    uint8_t reserved0;
    uint32_t bitrate;
    uint32_t accept_id;
    uint32_t accept_mask;
    uint32_t maximum_timeout_us;
    uint16_t reserved1;
} runa_can_resource_config_t;

typedef struct runa_can_hal {
    void *context;
    runa_status_t (*transmit)(void *context, uintptr_t handle,
                              const runa_can_frame_t *frame, uint32_t timeout_us);
    runa_status_t (*receive)(void *context, uintptr_t handle,
                             runa_can_frame_t *frame, uint32_t timeout_us);
} runa_can_hal_t;

runa_module_t runa_can_module(runa_can_hal_t *hal);

#endif
