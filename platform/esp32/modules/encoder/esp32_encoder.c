#include "esp32_encoder.h"

#include "esp_err.h"

#include <limits.h>
#include <stdint.h>

static runa_status_t map_encoder_error(esp_err_t status) {
    if (status == ESP_ERR_TIMEOUT) return RUNA_ERR_IO_TIMEOUT;
    if (status == ESP_ERR_INVALID_ARG) return RUNA_ERR_INVALID_FORMAT;
    if (status == ESP_ERR_INVALID_STATE) return RUNA_ERR_INTERNAL;
    return RUNA_ENCODER_ERR_IO;
}

runa_status_t runa_esp32_encoder_init(const runa_encoder_resource_config_t *configuration,
                                      runa_esp32_encoder_t *encoder) {
    pcnt_unit_config_t unit_configuration = {0};
    pcnt_chan_config_t channel_configuration = {0};
    esp_err_t status;
    if (configuration == NULL || encoder == NULL || configuration->a_pin < 0 ||
        configuration->b_pin < 0 || configuration->a_pin == configuration->b_pin ||
        configuration->decode_mode != RUNA_ENCODER_DECODE_X4 ||
        configuration->invert_direction > 1u || configuration->reserved != 0u)
        return RUNA_ERR_INVALID_FORMAT;

    *encoder = (runa_esp32_encoder_t){0};
    encoder->lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    encoder->reset_value = configuration->reset_value;
    unit_configuration.low_limit = INT16_MIN;
    unit_configuration.high_limit = INT16_MAX;
    unit_configuration.flags.accum_count = 1u;
    status = pcnt_new_unit(&unit_configuration, &encoder->unit);
    if (status != ESP_OK) return map_encoder_error(status);

    channel_configuration.edge_gpio_num = configuration->a_pin;
    channel_configuration.level_gpio_num = configuration->b_pin;
    status = pcnt_new_channel(encoder->unit, &channel_configuration, &encoder->channel_a);
    if (status != ESP_OK) return map_encoder_error(status);
    channel_configuration.edge_gpio_num = configuration->b_pin;
    channel_configuration.level_gpio_num = configuration->a_pin;
    status = pcnt_new_channel(encoder->unit, &channel_configuration, &encoder->channel_b);
    if (status != ESP_OK) return map_encoder_error(status);

    status = pcnt_channel_set_edge_action(
        encoder->channel_a,
        configuration->invert_direction == 0u ? PCNT_CHANNEL_EDGE_ACTION_INCREASE :
                                                 PCNT_CHANNEL_EDGE_ACTION_DECREASE,
        configuration->invert_direction == 0u ? PCNT_CHANNEL_EDGE_ACTION_DECREASE :
                                                 PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    if (status != ESP_OK) return map_encoder_error(status);
    status = pcnt_channel_set_level_action(
        encoder->channel_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    if (status != ESP_OK) return map_encoder_error(status);
    status = pcnt_channel_set_edge_action(
        encoder->channel_b,
        configuration->invert_direction == 0u ? PCNT_CHANNEL_EDGE_ACTION_DECREASE :
                                                 PCNT_CHANNEL_EDGE_ACTION_INCREASE,
        configuration->invert_direction == 0u ? PCNT_CHANNEL_EDGE_ACTION_INCREASE :
                                                 PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    if (status != ESP_OK) return map_encoder_error(status);
    status = pcnt_channel_set_level_action(
        encoder->channel_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    if (status != ESP_OK) return map_encoder_error(status);
    status = pcnt_unit_clear_count(encoder->unit);
    if (status != ESP_OK) return map_encoder_error(status);
    status = pcnt_unit_enable(encoder->unit);
    if (status != ESP_OK) return map_encoder_error(status);
    status = pcnt_unit_start(encoder->unit);
    return status == ESP_OK ? RUNA_OK : map_encoder_error(status);
}

static runa_status_t read_count(void *context, uintptr_t handle, int32_t *position,
                                uint8_t clear) {
    runa_esp32_encoder_t *encoder = (runa_esp32_encoder_t *)handle;
    int raw_count = 0;
    int64_t value;
    esp_err_t status;
    (void)context;
    if (encoder == NULL || position == NULL || encoder->unit == NULL) return RUNA_ERR_INVALID_FORMAT;
    portENTER_CRITICAL(&encoder->lock);
    status = pcnt_unit_get_count(encoder->unit, &raw_count);
    if (status == ESP_OK && clear != 0u) status = pcnt_unit_clear_count(encoder->unit);
    portEXIT_CRITICAL(&encoder->lock);
    if (status != ESP_OK) return map_encoder_error(status);
    value = (int64_t)encoder->reset_value + (int64_t)raw_count;
    if (value < INT32_MIN || value > INT32_MAX) return RUNA_ENCODER_ERR_OVERFLOW;
    *position = (int32_t)value;
    return RUNA_OK;
}

static runa_status_t read(void *context, uintptr_t handle, int32_t *position) {
    return read_count(context, handle, position, 0u);
}

static runa_status_t reset(void *context, uintptr_t handle) {
    runa_esp32_encoder_t *encoder = (runa_esp32_encoder_t *)handle;
    esp_err_t status;
    (void)context;
    if (encoder == NULL || encoder->unit == NULL) return RUNA_ERR_INVALID_FORMAT;
    portENTER_CRITICAL(&encoder->lock);
    status = pcnt_unit_clear_count(encoder->unit);
    portEXIT_CRITICAL(&encoder->lock);
    return status == ESP_OK ? RUNA_OK : map_encoder_error(status);
}

static runa_status_t read_reset(void *context, uintptr_t handle, int32_t *position) {
    return read_count(context, handle, position, 1u);
}

runa_encoder_hal_t runa_esp32_encoder_hal(void) {
    runa_encoder_hal_t hal = { NULL, read, reset, read_reset };
    return hal;
}
