#ifndef RUNA_ESP32_ENCODER_H
#define RUNA_ESP32_ENCODER_H

#include "driver/pulse_cnt.h"
#include "freertos/FreeRTOS.h"
#include "runa_encoder.h"

typedef struct runa_esp32_encoder {
    pcnt_unit_handle_t unit;
    pcnt_channel_handle_t channel_a;
    pcnt_channel_handle_t channel_b;
    portMUX_TYPE lock;
    int32_t reset_value;
} runa_esp32_encoder_t;

runa_status_t runa_esp32_encoder_init(const runa_encoder_resource_config_t *configuration,
                                      runa_esp32_encoder_t *encoder);
runa_encoder_hal_t runa_esp32_encoder_hal(void);

#endif
