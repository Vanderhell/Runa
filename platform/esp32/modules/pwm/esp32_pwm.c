#include "esp32_platform.h"

#include "driver/ledc.h"

static runa_status_t write_pwm(void *context, uintptr_t handle, uint32_t value) {
    ledc_channel_t channel = (ledc_channel_t)handle;
    uint32_t duty;
    (void)context;
    if (handle >= LEDC_CHANNEL_MAX || value > 10000u) return RUNA_PWM_ERR_IO;
    duty = (value * 8191u + 5000u) / 10000u;
    if (ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty) != ESP_OK) return RUNA_PWM_ERR_IO;
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, channel) == ESP_OK ? RUNA_OK : RUNA_PWM_ERR_IO;
}

int runa_esp32_pwm_init(void) {
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false
    };
    ledc_channel_config_t channel = {
        .gpio_num = GPIO_NUM_6,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0u,
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags = { .output_invert = 0 }
    };
    if (ledc_timer_config(&timer) != ESP_OK) return -1;
    if (ledc_channel_config(&channel) != ESP_OK) return -1;
    return 0;
}

runa_pwm_hal_t runa_esp32_pwm_hal(void) {
    runa_pwm_hal_t hal = { NULL, write_pwm };
    return hal;
}
