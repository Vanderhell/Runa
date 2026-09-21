#include "esp32_pulse.h"

#include "driver/pulse_cnt.h"
#include "driver/rmt_rx.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>

#define RUNA_ESP32_PULSE_SYMBOLS 64u
#define RUNA_ESP32_PULSE_MAX_TIMEOUT_US 1000000u

typedef struct runa_esp32_pulse_state {
    pcnt_unit_handle_t counter;
    pcnt_channel_handle_t counter_channel;
    rmt_channel_handle_t capture;
    volatile bool capture_done;
    size_t capture_symbols;
    rmt_symbol_word_t symbols[RUNA_ESP32_PULSE_SYMBOLS];
} runa_esp32_pulse_state_t;

static runa_esp32_pulse_state_t pulse_state;

static runa_status_t map_status(esp_err_t status) {
    if (status == ESP_OK) return RUNA_OK;
    if (status == ESP_ERR_TIMEOUT) return RUNA_ERR_IO_TIMEOUT;
    if (status == ESP_ERR_INVALID_ARG) return RUNA_ERR_INVALID_FORMAT;
    return RUNA_PULSE_ERR_IO;
}

static bool capture_done(rmt_channel_handle_t channel,
                         const rmt_rx_done_event_data_t *event, void *context) {
    runa_esp32_pulse_state_t *state = (runa_esp32_pulse_state_t *)context;
    (void)channel;
    if (state != NULL && event != NULL) {
        state->capture_symbols = event->num_symbols > RUNA_ESP32_PULSE_SYMBOLS ?
                                 RUNA_ESP32_PULSE_SYMBOLS : event->num_symbols;
        state->capture_done = true;
    }
    return false;
}

static runa_status_t reset_capture(runa_esp32_pulse_state_t *state) {
    esp_err_t status = rmt_disable(state->capture);
    if (status != ESP_OK) return map_status(status);
    state->capture_done = false;
    state->capture_symbols = 0u;
    return map_status(rmt_enable(state->capture));
}

static runa_status_t count_edges(void *context, uintptr_t handle, uint8_t edge,
                                 uint32_t window_us, uint32_t max_count, uint32_t *count) {
    runa_esp32_pulse_state_t *state = (runa_esp32_pulse_state_t *)context;
    pcnt_channel_edge_action_t positive = PCNT_CHANNEL_EDGE_ACTION_HOLD;
    pcnt_channel_edge_action_t negative = PCNT_CHANNEL_EDGE_ACTION_HOLD;
    int value = 0;
    uint64_t start;
    (void)handle;
    if (state == NULL || count == NULL || edge > RUNA_PULSE_EDGE_BOTH || max_count == 0u)
        return RUNA_ERR_INVALID_FORMAT;
    if (edge == RUNA_PULSE_EDGE_RISING || edge == RUNA_PULSE_EDGE_BOTH)
        positive = PCNT_CHANNEL_EDGE_ACTION_INCREASE;
    if (edge == RUNA_PULSE_EDGE_FALLING || edge == RUNA_PULSE_EDGE_BOTH)
        negative = PCNT_CHANNEL_EDGE_ACTION_INCREASE;
    if (pcnt_unit_clear_count(state->counter) != ESP_OK ||
        pcnt_channel_set_edge_action(state->counter_channel, positive, negative) != ESP_OK ||
        pcnt_unit_enable(state->counter) != ESP_OK || pcnt_unit_start(state->counter) != ESP_OK)
        return RUNA_PULSE_ERR_IO;
    start = (uint64_t)esp_timer_get_time();
    while ((uint64_t)esp_timer_get_time() - start < window_us) {
        if (pcnt_unit_get_count(state->counter, &value) != ESP_OK) {
            (void)pcnt_unit_stop(state->counter);
            return RUNA_PULSE_ERR_IO;
        }
        if (value >= 0 && (uint32_t)value >= max_count) break;
        esp_rom_delay_us(100u);
    }
    if (pcnt_unit_stop(state->counter) != ESP_OK || pcnt_unit_get_count(state->counter, &value) != ESP_OK)
        return RUNA_PULSE_ERR_IO;
    *count = value < 0 ? 0u : (uint32_t)value;
    return RUNA_OK;
}

static runa_status_t receive_symbols(runa_esp32_pulse_state_t *state, uint32_t timeout_us) {
    rmt_receive_config_t receive_config = {
        .signal_range_min_ns = 1000u,
        .signal_range_max_ns = 1000000000u
    };
    uint64_t start;
    runa_status_t status;
    if (timeout_us == 0u || timeout_us > RUNA_ESP32_PULSE_MAX_TIMEOUT_US)
        return RUNA_ERR_OUT_OF_RANGE;
    status = reset_capture(state);
    if (status != RUNA_OK) return status;
    status = map_status(rmt_receive(state->capture, state->symbols,
                                    sizeof state->symbols, &receive_config));
    if (status != RUNA_OK) return status;
    start = (uint64_t)esp_timer_get_time();
    while (!state->capture_done) {
        if ((uint64_t)esp_timer_get_time() - start >= timeout_us) {
            (void)rmt_disable(state->capture);
            return RUNA_ERR_IO_TIMEOUT;
        }
        vTaskDelay(1u);
    }
    return RUNA_OK;
}

static size_t flatten_symbols(const runa_esp32_pulse_state_t *state, uint8_t *levels,
                              uint32_t *durations, size_t capacity) {
    size_t output = 0u;
    size_t index;
    if (state == NULL || levels == NULL || durations == NULL) return 0u;
    for (index = 0u; index < state->capture_symbols && output + 1u < capacity; ++index) {
        levels[output] = state->symbols[index].level0;
        durations[output++] = state->symbols[index].duration0;
        levels[output] = state->symbols[index].level1;
        durations[output++] = state->symbols[index].duration1;
    }
    return output;
}

static runa_status_t measure_width(void *context, uintptr_t handle, uint8_t level,
                                   uint32_t timeout_us, uint32_t *width_us) {
    runa_esp32_pulse_state_t *state = (runa_esp32_pulse_state_t *)context;
    uint8_t levels[RUNA_ESP32_PULSE_SYMBOLS * 2u];
    uint32_t durations[RUNA_ESP32_PULSE_SYMBOLS * 2u];
    size_t count;
    size_t index;
    runa_status_t status;
    (void)handle;
    if (state == NULL || width_us == NULL ||
        (level != RUNA_PULSE_LEVEL_HIGH && level != RUNA_PULSE_LEVEL_LOW))
        return RUNA_ERR_INVALID_FORMAT;
    status = receive_symbols(state, timeout_us);
    if (status != RUNA_OK) return status;
    count = flatten_symbols(state, levels, durations, sizeof levels / sizeof levels[0]);
    for (index = 1u; index + 1u < count; ++index) {
        if (levels[index] == (level == RUNA_PULSE_LEVEL_HIGH ? 1u : 0u) &&
            levels[index - 1u] != levels[index] && levels[index + 1u] != levels[index] &&
            durations[index] != 0u) {
            *width_us = durations[index];
            return RUNA_OK;
        }
    }
    return RUNA_ERR_IO_TIMEOUT;
}

static runa_status_t measure_period(void *context, uintptr_t handle, uint8_t edge,
                                    uint32_t timeout_us, uint32_t *period_us) {
    runa_esp32_pulse_state_t *state = (runa_esp32_pulse_state_t *)context;
    uint8_t levels[RUNA_ESP32_PULSE_SYMBOLS * 2u];
    uint32_t durations[RUNA_ESP32_PULSE_SYMBOLS * 2u];
    size_t count;
    size_t index;
    uint64_t elapsed = 0u;
    uint64_t first = 0u;
    bool found = false;
    uint8_t target = edge == RUNA_PULSE_EDGE_RISING ? 1u : 0u;
    runa_status_t status;
    (void)handle;
    if (state == NULL || period_us == NULL || edge > RUNA_PULSE_EDGE_FALLING)
        return RUNA_ERR_INVALID_FORMAT;
    status = receive_symbols(state, timeout_us);
    if (status != RUNA_OK) return status;
    count = flatten_symbols(state, levels, durations, sizeof levels / sizeof levels[0]);
    for (index = 1u; index < count; ++index) {
        if (levels[index] == target && levels[index - 1u] != target) {
            if (!found) {
                first = elapsed;
                found = true;
            } else if (elapsed >= first && elapsed - first <= UINT32_MAX) {
                *period_us = (uint32_t)(elapsed - first);
                return *period_us == 0u ? RUNA_PULSE_ERR_INVALID_MEASUREMENT : RUNA_OK;
            }
        }
        elapsed += durations[index];
    }
    return RUNA_ERR_IO_TIMEOUT;
}

int runa_esp32_pulse_init(int gpio_num) {
    pcnt_unit_config_t unit_config = { .low_limit = 0, .high_limit = 32767 };
    pcnt_chan_config_t channel_config = { .edge_gpio_num = gpio_num, .level_gpio_num = -1 };
    rmt_rx_channel_config_t rx_config = {
        .gpio_num = gpio_num, .clk_src = RMT_CLK_SRC_DEFAULT, .resolution_hz = 1000000u,
        .mem_block_symbols = RUNA_ESP32_PULSE_SYMBOLS, .flags.invert_in = 0,
        .flags.with_dma = 0
    };
    rmt_rx_event_callbacks_t callbacks = { .on_recv_done = capture_done };
    if (gpio_num < 0 || pcnt_new_unit(&unit_config, &pulse_state.counter) != ESP_OK ||
        pcnt_new_channel(pulse_state.counter, &channel_config, &pulse_state.counter_channel) != ESP_OK ||
        rmt_new_rx_channel(&rx_config, &pulse_state.capture) != ESP_OK ||
        rmt_rx_register_event_callbacks(pulse_state.capture, &callbacks, &pulse_state) != ESP_OK ||
        rmt_enable(pulse_state.capture) != ESP_OK)
        return -1;
    return 0;
}

runa_pulse_hal_t runa_esp32_pulse_hal(void) {
    runa_pulse_hal_t hal = { &pulse_state, count_edges, measure_width, measure_period };
    return hal;
}
