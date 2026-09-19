#ifndef RUNA_UART_H
#define RUNA_UART_H

#include "runa_module.h"

#define RUNA_UART_MODULE_ID 6u
#define RUNA_UART_RESOURCE_TYPE 1u
#define RUNA_UART_OP_TRANSFER 1u
#define RUNA_UART_MAX_TX_BYTES 240u
#define RUNA_UART_MAX_RX_BYTES 240u
#define RUNA_UART_MAX_TIMEOUT_US 5000000u
#define RUNA_UART_MIN_BAUD 300u
#define RUNA_UART_MAX_BAUD 5000000u

enum {
    RUNA_UART_RX_FIXED_LENGTH = 0u,
    RUNA_UART_RX_UP_TO_LENGTH = 1u
};
enum { RUNA_UART_PARITY_NONE = 0u, RUNA_UART_PARITY_EVEN = 1u, RUNA_UART_PARITY_ODD = 2u };
enum { RUNA_UART_FLOW_NONE = 0u, RUNA_UART_FLOW_RTS = 1u, RUNA_UART_FLOW_CTS = 2u,
       RUNA_UART_FLOW_RTS_CTS = 3u };
enum { RUNA_UART_ERR_IO = 27u, RUNA_UART_ERR_PARTIAL = 28u };

typedef struct runa_uart_resource_config {
    uint8_t controller;
    uint8_t data_bits;
    uint8_t stop_bits;
    uint8_t parity;
    uint8_t flow_control;
    uint8_t reserved0;
    int16_t tx_pin;
    int16_t rx_pin;
    uint32_t baud_rate;
    uint16_t maximum_tx_bytes;
    uint16_t maximum_rx_bytes;
    uint32_t maximum_timeout_us;
    uint16_t reserved1;
} runa_uart_resource_config_t;

typedef struct runa_uart_hal {
    void *context;
    runa_status_t (*transfer)(void *context, uintptr_t handle,
                              const runa_uart_resource_config_t *configuration,
                              const uint8_t *transmit, size_t transmit_size,
                              uint8_t *receive, size_t receive_capacity, size_t *receive_size,
                              uint8_t receive_policy, uint32_t timeout_us);
} runa_uart_hal_t;

runa_module_t runa_uart_module(runa_uart_hal_t *hal);

#endif
