#include "esp32_uart.h"

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static runa_status_t map_uart_error(esp_err_t status) {
    if (status == ESP_ERR_TIMEOUT) return RUNA_ERR_IO_TIMEOUT;
    if (status == ESP_ERR_INVALID_ARG) return RUNA_ERR_INVALID_FORMAT;
    return RUNA_UART_ERR_IO;
}

static uart_word_length_t word_length(uint8_t data_bits) {
    switch (data_bits) {
    case 5u: return UART_DATA_5_BITS;
    case 6u: return UART_DATA_6_BITS;
    case 7u: return UART_DATA_7_BITS;
    default: return UART_DATA_8_BITS;
    }
}

runa_status_t runa_esp32_uart_init(const runa_uart_resource_config_t *configuration,
                                   uart_port_t *port) {
    uart_config_t uart_configuration = {0};
    esp_err_t status;
    if (configuration == NULL || port == NULL || configuration->controller >= UART_NUM_MAX)
        return RUNA_ERR_INVALID_FORMAT;
    uart_configuration.baud_rate = (int)configuration->baud_rate;
    uart_configuration.data_bits = word_length(configuration->data_bits);
    uart_configuration.parity = configuration->parity == RUNA_UART_PARITY_NONE ? UART_PARITY_DISABLE :
                                configuration->parity == RUNA_UART_PARITY_EVEN ? UART_PARITY_EVEN : UART_PARITY_ODD;
    uart_configuration.stop_bits = configuration->stop_bits == 1u ? UART_STOP_BITS_1 : UART_STOP_BITS_2;
    uart_configuration.flow_ctrl = (uart_hw_flowcontrol_t)configuration->flow_control;
    uart_configuration.source_clk = UART_SCLK_DEFAULT;
    status = uart_param_config((uart_port_t)configuration->controller, &uart_configuration);
    if (status != ESP_OK) return map_uart_error(status);
    status = uart_set_pin((uart_port_t)configuration->controller, configuration->tx_pin,
                          configuration->rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (status != ESP_OK) return map_uart_error(status);
    status = uart_driver_install((uart_port_t)configuration->controller,
                                 (int)configuration->maximum_rx_bytes * 2,
                                 (int)configuration->maximum_tx_bytes * 2, 0, NULL, 0);
    if (status != ESP_OK) return map_uart_error(status);
    *port = (uart_port_t)configuration->controller;
    return RUNA_OK;
}

static runa_status_t transfer(void *context, uintptr_t handle,
                              const runa_uart_resource_config_t *configuration,
                              const uint8_t *transmit, size_t transmit_size,
                              uint8_t *receive, size_t receive_capacity, size_t *receive_size,
                              uint8_t receive_policy, uint32_t timeout_us) {
    uart_port_t port = (uart_port_t)handle;
    TickType_t timeout_ticks = 0;
    int written;
    int received;
    (void)context;
    (void)configuration;
    if (receive_size == NULL || port >= UART_NUM_MAX ||
        (transmit_size != 0u && transmit == NULL) ||
        (receive_capacity != 0u && receive == NULL) ||
        (receive_capacity == 0u && receive_policy != RUNA_UART_RX_FIXED_LENGTH))
        return RUNA_ERR_INVALID_FORMAT;
    *receive_size = 0u;
    if (timeout_us != 0u) {
        uint32_t timeout_ms = (timeout_us + 999u) / 1000u;
        timeout_ticks = pdMS_TO_TICKS(timeout_ms);
        if (timeout_ticks == 0) timeout_ticks = 1;
    }
    if (transmit_size != 0u) {
        written = uart_write_bytes(port, (const char *)transmit, transmit_size);
        if (written < 0 || (size_t)written != transmit_size) return RUNA_UART_ERR_IO;
        if (timeout_ticks != 0 && uart_wait_tx_done(port, timeout_ticks) != ESP_OK)
            return RUNA_ERR_IO_TIMEOUT;
    }
    if (receive_capacity == 0u) return RUNA_OK;
    received = uart_read_bytes(port, receive, receive_capacity, timeout_ticks);
    if (received < 0) return RUNA_UART_ERR_IO;
    *receive_size = (size_t)received;
    if (receive_policy == RUNA_UART_RX_FIXED_LENGTH && *receive_size != receive_capacity)
        return RUNA_ERR_IO_TIMEOUT;
    return RUNA_OK;
}

runa_uart_hal_t runa_esp32_uart_hal(void) {
    runa_uart_hal_t hal = { NULL, transfer };
    return hal;
}
