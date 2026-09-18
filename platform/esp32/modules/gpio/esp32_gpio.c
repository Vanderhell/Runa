#include "esp32_platform.h"

#include "driver/gpio.h"

static runa_status_t read_gpio(void *context, uintptr_t handle, uint32_t *value) {
    int level;
    (void)context;
    if (value == NULL || handle >= (uintptr_t)GPIO_NUM_MAX) return RUNA_GPIO_ERR_IO;
    level = gpio_get_level((gpio_num_t)handle);
    *value = level != 0 ? 1u : 0u;
    return RUNA_OK;
}

static runa_status_t write_gpio(void *context, uintptr_t handle, uint32_t value) {
    (void)context;
    if (handle >= (uintptr_t)GPIO_NUM_MAX || value > 1u) return RUNA_GPIO_ERR_IO;
    return gpio_set_level((gpio_num_t)handle, value) == ESP_OK ? RUNA_OK : RUNA_GPIO_ERR_IO;
}

int runa_esp32_gpio_init(void) {
    gpio_config_t output = {
        .pin_bit_mask = 1ULL << GPIO_NUM_4,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config_t input = {
        .pin_bit_mask = 1ULL << GPIO_NUM_5,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    if (gpio_config(&output) != ESP_OK || gpio_set_level(GPIO_NUM_4, 0u) != ESP_OK) return -1;
    if (gpio_config(&input) != ESP_OK) return -1;
    return 0;
}

runa_gpio_hal_t runa_esp32_gpio_hal(void) {
    runa_gpio_hal_t hal = { NULL, read_gpio, write_gpio };
    return hal;
}
