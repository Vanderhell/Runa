#ifndef RUNA_ESP32_CAN_H
#define RUNA_ESP32_CAN_H

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#include "driver/twai.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "runa_can.h"

runa_status_t runa_esp32_can_init(const runa_can_resource_config_t *configuration,
                                  int tx_pin, int rx_pin, twai_handle_t *handle);
runa_can_hal_t runa_esp32_can_hal(void);

#endif
