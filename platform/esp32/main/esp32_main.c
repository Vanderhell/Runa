#include "esp32_platform.h"

#include "esp_err.h"
#include "esp32_spi.h"
#include "esp32_i2c.h"
#include "esp32_uart.h"
#include "esp32_can.h"
#include "esp32_pulse.h"
#include "esp32_encoder.h"

static const runa_adc_resource_config_t adc_resource_config = { 4095u };
static const runa_pwm_resource_config_t pwm_resource_config = { 10000u };
static const runa_spi_resource_config_t spi_resource_config = {
    0u, RUNA_SPI_CS_AUTOMATIC, RUNA_SPI_MAX_TRANSFER_BYTES, 1000000u,
    100000u, 10000000u, RUNA_SPI_MAX_TIMEOUT_MS, 0u, 10u
};
static const runa_i2c_resource_config_t i2c_resource_config = {
    0u, 0x50u, RUNA_I2C_MAX_TX_BYTES, RUNA_I2C_MAX_RX_BYTES, 100000u,
    RUNA_I2C_MAX_TIMEOUT_MS, 0u
};
static const runa_uart_resource_config_t uart_resource_config = {
    1u, 8u, 1u, RUNA_UART_PARITY_NONE, RUNA_UART_FLOW_NONE, 0u,
    17, 18, 115200u, 64u, 64u, 1000000u, 0u
};
static const runa_can_resource_config_t can_resource_config = {
    0u, RUNA_CAN_FILTER_STANDARD, 0u, 0u, 500000u, 0u, 0u, 1000000u, 0u
};
static const runa_pulse_resource_config_t pulse_resource_config = {
    1u, 1000000u, 1u, 1000000u, 32767u, 7u, 3u, 0u
};
static const runa_encoder_resource_config_t encoder_resource_config = {
    25, 26, RUNA_ENCODER_DECODE_X4, 0u, 0, 0u
};
static runa_esp32_encoder_t encoder_device;
static spi_device_handle_t spi_device;
static i2c_master_dev_handle_t i2c_device;
static runa_resource_t resources[] = {
    { 1u, RUNA_GPIO_MODULE_ID, RUNA_GPIO_RESOURCE_TYPE, 0u, RUNA_PERMISSION_WRITE, 4u, NULL },
    { 2u, RUNA_GPIO_MODULE_ID, RUNA_GPIO_RESOURCE_TYPE, 0u, RUNA_PERMISSION_READ, 5u, NULL },
    { 3u, RUNA_ADC_MODULE_ID, RUNA_ADC_RESOURCE_TYPE, 0u, RUNA_PERMISSION_READ, 0u, &adc_resource_config },
    { 4u, RUNA_PWM_MODULE_ID, RUNA_PWM_RESOURCE_TYPE, 0u, RUNA_PERMISSION_WRITE, 0u, &pwm_resource_config },
    { 9u, RUNA_SPI_MODULE_ID, RUNA_SPI_RESOURCE_TYPE, 0u,
      RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE, (uintptr_t)&spi_device, &spi_resource_config },
    { 10u, RUNA_I2C_MODULE_ID, RUNA_I2C_RESOURCE_TYPE, 0u,
      RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE, (uintptr_t)&i2c_device, &i2c_resource_config },
    { 5u, RUNA_UART_MODULE_ID, RUNA_UART_RESOURCE_TYPE, 0u,
      RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE, 0u, &uart_resource_config },
    { 6u, RUNA_CAN_MODULE_ID, RUNA_CAN_RESOURCE_TYPE, 0u,
      RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE, 0u, &can_resource_config },
    { 7u, RUNA_PULSE_MODULE_ID, RUNA_PULSE_RESOURCE_TYPE, 0u,
      RUNA_PERMISSION_READ, 7u, &pulse_resource_config },
    { 8u, RUNA_ENCODER_MODULE_ID, RUNA_ENCODER_RESOURCE_TYPE, 0u,
      RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE, (uintptr_t)&encoder_device,
      &encoder_resource_config }
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
    runa_spi_hal_t spi_hal;
    runa_i2c_hal_t i2c_hal;
    runa_uart_hal_t uart_hal;
    uart_port_t uart_port;
    runa_can_hal_t can_hal;
    twai_handle_t can_handle;
    runa_pulse_hal_t pulse_hal;
    runa_encoder_hal_t encoder_hal;
    runa_module_t gpio_module;
    runa_module_t adc_module;
    runa_module_t pwm_module;
    runa_module_t spi_module;
    runa_module_t i2c_module;
    runa_module_t uart_module;
    runa_module_t can_module;
    runa_module_t pulse_module;
    runa_module_t encoder_module;
    runa_platform_t platform;
    runa_event_sink_t sink = { event_sink, NULL };

    ESP_ERROR_CHECK(runa_esp32_gpio_init() == 0 ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runa_esp32_adc_init() == 0 ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runa_esp32_pwm_init() == 0 ? ESP_OK : ESP_FAIL);
    {
        spi_bus_config_t bus = {
            .mosi_io_num = 11,
            .miso_io_num = 12,
            .sclk_io_num = 13,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = RUNA_SPI_MAX_TRANSFER_BYTES
        };
        ESP_ERROR_CHECK(runa_esp32_spi_bus_initialize(SPI2_HOST, &bus) == RUNA_OK ? ESP_OK : ESP_FAIL);
    }
    ESP_ERROR_CHECK(runa_esp32_spi_device_add(SPI2_HOST, &spi_resource_config, &spi_device) == RUNA_OK ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runa_esp32_i2c_bus_initialize(0u, 8, 9) == RUNA_OK ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runa_esp32_i2c_device_add(0u, &i2c_resource_config, &i2c_device) == RUNA_OK ? ESP_OK : ESP_FAIL);
    resources[4].platform_handle = (uintptr_t)spi_device;
    resources[5].platform_handle = (uintptr_t)i2c_device;
    ESP_ERROR_CHECK(runa_esp32_uart_init(&uart_resource_config, &uart_port) == RUNA_OK ? ESP_OK : ESP_FAIL);
    resources[6].platform_handle = (uintptr_t)uart_port;
    ESP_ERROR_CHECK(runa_esp32_can_init(&can_resource_config, 21, 22, &can_handle) == RUNA_OK ? ESP_OK : ESP_FAIL);
    resources[7].platform_handle = (uintptr_t)can_handle;
    ESP_ERROR_CHECK(runa_esp32_pulse_init(7) == 0 ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runa_esp32_encoder_init(&encoder_resource_config, &encoder_device) == RUNA_OK ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runa_esp32_transport_init() == 0 ? ESP_OK : ESP_FAIL);
    gpio_hal = runa_esp32_gpio_hal();
    adc_hal = runa_esp32_adc_hal();
    pwm_hal = runa_esp32_pwm_hal();
    spi_hal = runa_esp32_spi_hal();
    i2c_hal = runa_esp32_i2c_hal();
    uart_hal = runa_esp32_uart_hal();
    can_hal = runa_esp32_can_hal();
    pulse_hal = runa_esp32_pulse_hal();
    encoder_hal = runa_esp32_encoder_hal();
    gpio_module = runa_gpio_module(&gpio_hal);
    adc_module = runa_adc_module(&adc_hal);
    pwm_module = runa_pwm_module(&pwm_hal);
    spi_module = runa_spi_module(&spi_hal);
    i2c_module = runa_i2c_module(&i2c_hal);
    uart_module = runa_uart_module(&uart_hal);
    can_module = runa_can_module(&can_hal);
    pulse_module = runa_pulse_module(&pulse_hal);
    encoder_module = runa_encoder_module(&encoder_hal);
    platform = runa_esp32_platform();
    runa_registry_init(&registry);
    ESP_ERROR_CHECK(runa_registry_add(&registry, &gpio_module) == RUNA_OK ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runa_registry_add(&registry, &adc_module) == RUNA_OK ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runa_registry_add(&registry, &pwm_module) == RUNA_OK ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runa_registry_add(&registry, &spi_module) == RUNA_OK ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runa_registry_add(&registry, &i2c_module) == RUNA_OK ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runa_registry_add(&registry, &uart_module) == RUNA_OK ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runa_registry_add(&registry, &can_module) == RUNA_OK ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runa_registry_add(&registry, &pulse_module) == RUNA_OK ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runa_registry_add(&registry, &encoder_module) == RUNA_OK ? ESP_OK : ESP_FAIL);
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
