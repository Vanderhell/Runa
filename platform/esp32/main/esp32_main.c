#include "esp32_platform.h"

#include "esp_err.h"

static const runa_adc_resource_config_t adc_resource_config = { 4095u };
static const runa_pwm_resource_config_t pwm_resource_config = { 10000u };
static const runa_resource_t resources[] = {
    { 1u, RUNA_GPIO_MODULE_ID, RUNA_GPIO_RESOURCE_TYPE, 0u, RUNA_PERMISSION_WRITE, 4u, NULL },
    { 2u, RUNA_GPIO_MODULE_ID, RUNA_GPIO_RESOURCE_TYPE, 0u, RUNA_PERMISSION_READ, 5u, NULL },
    { 3u, RUNA_ADC_MODULE_ID, RUNA_ADC_RESOURCE_TYPE, 0u, RUNA_PERMISSION_READ, 0u, &adc_resource_config },
    { 4u, RUNA_PWM_MODULE_ID, RUNA_PWM_RESOURCE_TYPE, 0u, RUNA_PERMISSION_WRITE, 0u, &pwm_resource_config }
};

const runa_resource_table_t runa_esp32_resources = {
    resources, sizeof resources / sizeof resources[0]
};

static int event_sink(void *context, const uint8_t *event, size_t size) {
    (void)context;
    return runa_esp32_transport_write(event, size);
}

void app_main(void) {
    static uint8_t payload[RUNA_MAX_JOB_BYTES];
    runa_module_registry_t registry;
    runa_gpio_hal_t gpio_hal;
    runa_adc_hal_t adc_hal;
    runa_pwm_hal_t pwm_hal;
    runa_module_t gpio_module;
    runa_module_t adc_module;
    runa_module_t pwm_module;
    runa_platform_t platform;
    runa_event_sink_t sink = { event_sink, NULL };

    ESP_ERROR_CHECK(runa_esp32_gpio_init() == 0 ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runa_esp32_adc_init() == 0 ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runa_esp32_pwm_init() == 0 ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runa_esp32_transport_init() == 0 ? ESP_OK : ESP_FAIL);
    gpio_hal = runa_esp32_gpio_hal();
    adc_hal = runa_esp32_adc_hal();
    pwm_hal = runa_esp32_pwm_hal();
    gpio_module = runa_gpio_module(&gpio_hal);
    adc_module = runa_adc_module(&adc_hal);
    pwm_module = runa_pwm_module(&pwm_hal);
    platform = runa_esp32_platform();
    runa_registry_init(&registry);
    ESP_ERROR_CHECK(runa_registry_add(&registry, &gpio_module) == RUNA_OK ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runa_registry_add(&registry, &adc_module) == RUNA_OK ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runa_registry_add(&registry, &pwm_module) == RUNA_OK ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runa_resource_table_validate(&runa_esp32_resources, &registry) == RUNA_OK ? ESP_OK : ESP_FAIL);

    for (;;) {
        uint8_t command = 0u;
        size_t payload_size = 0u;
        if (runa_esp32_transport_read_request(&command, payload, sizeof payload, &payload_size) != 0)
            continue;
        switch (command) {
        case RUNA_COMMAND_GET_INFO:
            if (payload_size == 0u) runa_esp32_send_info();
            break;
        case RUNA_COMMAND_GET_CAPABILITIES_V1:
            if (payload_size == 0u) runa_esp32_send_capabilities_v1();
            break;
        case RUNA_COMMAND_GET_CAPABILITIES_V2:
            if (payload_size == 0u) runa_esp32_send_capabilities_v2(&registry);
            break;
        case RUNA_COMMAND_EXECUTE:
            (void)runa_process(payload, payload_size, &runa_esp32_resources, &registry, &platform, &sink);
            break;
        default:
            break;
        }
    }
}
