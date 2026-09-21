#include "esp32_can.h"

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static runa_status_t map_can_error(esp_err_t status) {
    if (status == ESP_ERR_TIMEOUT) return RUNA_ERR_IO_TIMEOUT;
    if (status == ESP_ERR_INVALID_STATE) return RUNA_CAN_ERR_CONTROLLER;
    if (status == ESP_ERR_INVALID_ARG) return RUNA_ERR_INVALID_FORMAT;
    return RUNA_CAN_ERR_IO;
}

static twai_timing_config_t timing(uint32_t bitrate) {
    switch (bitrate) {
    case 125000u: return (twai_timing_config_t)TWAI_TIMING_CONFIG_125KBITS();
    case 250000u: return (twai_timing_config_t)TWAI_TIMING_CONFIG_250KBITS();
    case 500000u: return (twai_timing_config_t)TWAI_TIMING_CONFIG_500KBITS();
    case 1000000u: return (twai_timing_config_t)TWAI_TIMING_CONFIG_1MBITS();
    default: return (twai_timing_config_t)TWAI_TIMING_CONFIG_500KBITS();
    }
}

runa_status_t runa_esp32_can_init(const runa_can_resource_config_t *configuration,
                                  int tx_pin, int rx_pin, twai_handle_t *handle) {
    twai_general_config_t general = TWAI_GENERAL_CONFIG_DEFAULT(tx_pin, rx_pin, TWAI_MODE_NORMAL);
    twai_timing_config_t timing_config;
    twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    esp_err_t status;
    if (configuration == NULL || handle == NULL || configuration->controller != 0u ||
        (configuration->bitrate != 125000u && configuration->bitrate != 250000u &&
         configuration->bitrate != 500000u && configuration->bitrate != 1000000u))
        return RUNA_ERR_INVALID_FORMAT;
    general.mode = configuration->listen_only != 0u ? TWAI_MODE_LISTEN_ONLY : TWAI_MODE_NORMAL;
    general.tx_queue_len = 1u;
    general.rx_queue_len = 1u;
    timing_config = timing(configuration->bitrate);
    status = twai_driver_install_v2(&general, &timing_config, &filter, handle);
    if (status != ESP_OK) return map_can_error(status);
    status = twai_start_v2(*handle);
    return status == ESP_OK ? RUNA_OK : map_can_error(status);
}

static runa_status_t transmit(void *context, uintptr_t handle,
                              const runa_can_frame_t *frame, uint32_t timeout_us) {
    twai_message_t message = {0};
    esp_err_t status;
    TickType_t ticks;
    (void)context;
    if (frame == NULL || frame->length > RUNA_CAN_MAX_PAYLOAD_BYTES ||
        (frame->flags & (uint8_t)~RUNA_CAN_FRAME_EXTENDED) != 0u || timeout_us == 0u)
        return RUNA_ERR_INVALID_FORMAT;
    message.identifier = frame->id;
    message.extd = (frame->flags & RUNA_CAN_FRAME_EXTENDED) != 0u;
    message.data_length_code = frame->length;
    memcpy(message.data, frame->data, frame->length);
    ticks = pdMS_TO_TICKS((timeout_us + 999u) / 1000u);
    if (ticks == 0) ticks = 1;
    status = twai_transmit_v2((twai_handle_t)handle, &message, ticks);
    return status == ESP_OK ? RUNA_OK : map_can_error(status);
}

static runa_status_t receive(void *context, uintptr_t handle,
                             runa_can_frame_t *frame, uint32_t timeout_us) {
    twai_message_t message = {0};
    esp_err_t status;
    TickType_t ticks;
    (void)context;
    if (frame == NULL || timeout_us == 0u) return RUNA_ERR_INVALID_FORMAT;
    ticks = pdMS_TO_TICKS((timeout_us + 999u) / 1000u);
    if (ticks == 0) ticks = 1;
    status = twai_receive_v2((twai_handle_t)handle, &message, ticks);
    if (status != ESP_OK) return map_can_error(status);
    if (message.rtr || message.data_length_code > RUNA_CAN_MAX_PAYLOAD_BYTES) return RUNA_CAN_ERR_IO;
    frame->id = message.identifier;
    frame->flags = message.extd != 0u ? RUNA_CAN_FRAME_EXTENDED : 0u;
    frame->length = message.data_length_code;
    memcpy(frame->data, message.data, frame->length);
    return RUNA_OK;
}

runa_can_hal_t runa_esp32_can_hal(void) {
    runa_can_hal_t hal = { NULL, transmit, receive };
    return hal;
}
