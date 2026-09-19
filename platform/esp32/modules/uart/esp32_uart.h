#ifndef RUNA_ESP32_UART_H
#define RUNA_ESP32_UART_H

#include "driver/uart.h"
#include "runa_uart.h"

runa_status_t runa_esp32_uart_init(const runa_uart_resource_config_t *configuration,
                                   uart_port_t *port);
runa_uart_hal_t runa_esp32_uart_hal(void);

#endif
