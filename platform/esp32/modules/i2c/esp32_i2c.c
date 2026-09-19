#include "esp32_i2c.h"

#include "esp_err.h"

#include <stddef.h>

static i2c_master_bus_handle_t buses[2];

static runa_status_t map_i2c_error(esp_err_t status) {
    if (status == ESP_ERR_TIMEOUT) return RUNA_ERR_IO_TIMEOUT;
    if (status == ESP_ERR_INVALID_ARG) return RUNA_ERR_INVALID_FORMAT;
    return RUNA_I2C_ERR_BUS;
}

runa_status_t runa_esp32_i2c_bus_initialize(uint8_t bus, int sda_pin, int scl_pin) {
    i2c_master_bus_config_t bus_configuration = {0};
    esp_err_t status;
    if (bus >= (uint8_t)(sizeof buses / sizeof buses[0]) || sda_pin < 0 || scl_pin < 0)
        return RUNA_ERR_INVALID_FORMAT;
    bus_configuration.i2c_port = (i2c_port_num_t)bus;
    bus_configuration.sda_io_num = (gpio_num_t)sda_pin;
    bus_configuration.scl_io_num = (gpio_num_t)scl_pin;
    bus_configuration.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_configuration.glitch_ignore_cnt = 7u;
    bus_configuration.flags.enable_internal_pullup = true;
    status = i2c_new_master_bus(&bus_configuration, &buses[bus]);
    return status == ESP_OK ? RUNA_OK : map_i2c_error(status);
}

runa_status_t runa_esp32_i2c_device_add(uint8_t bus, const runa_i2c_resource_config_t *configuration,
                                        i2c_master_dev_handle_t *device) {
    i2c_device_config_t device_configuration = {0};
    esp_err_t status;
    if (bus >= (uint8_t)(sizeof buses / sizeof buses[0]) || buses[bus] == NULL ||
        configuration == NULL || device == NULL || configuration->address == 0u ||
        configuration->address > 0x7fu || configuration->clock_hz == 0u)
        return RUNA_ERR_INVALID_FORMAT;
    device_configuration.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    device_configuration.device_address = configuration->address;
    device_configuration.scl_speed_hz = configuration->clock_hz;
    status = i2c_master_bus_add_device(buses[bus], &device_configuration, device);
    return status == ESP_OK ? RUNA_OK : map_i2c_error(status);
}

static runa_status_t transfer(void *context, uintptr_t device_handle,
                              const runa_i2c_resource_config_t *configuration,
                              const uint8_t *transmit, size_t transmit_size,
                              uint8_t *receive, size_t receive_size,
                              uint16_t timeout_ms) {
    i2c_master_dev_handle_t device = (i2c_master_dev_handle_t)device_handle;
    esp_err_t status;
    (void)context;
    if (device == NULL || configuration == NULL || timeout_ms == 0u ||
        (transmit_size != 0u && transmit == NULL) || (receive_size != 0u && receive == NULL) ||
        (transmit_size == 0u && receive_size == 0u)) return RUNA_ERR_INVALID_FORMAT;
    if (transmit_size != 0u && receive_size != 0u)
        status = i2c_master_transmit_receive(device, transmit, transmit_size, receive, receive_size,
                                             (int)timeout_ms);
    else if (transmit_size != 0u)
        status = i2c_master_transmit(device, transmit, transmit_size, (int)timeout_ms);
    else
        status = i2c_master_receive(device, receive, receive_size, (int)timeout_ms);
    return status == ESP_OK ? RUNA_OK : map_i2c_error(status);
}

runa_i2c_hal_t runa_esp32_i2c_hal(void) {
    runa_i2c_hal_t hal = { NULL, transfer };
    return hal;
}
