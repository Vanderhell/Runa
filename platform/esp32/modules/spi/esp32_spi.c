#include "esp32_spi.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"

#include <limits.h>
#include <string.h>

static uint8_t transmit_staging[RUNA_SPI_MAX_TRANSFER_BYTES];
static uint8_t receive_staging[RUNA_SPI_MAX_TRANSFER_BYTES];

static runa_status_t map_spi_error(esp_err_t status) {
    return status == ESP_ERR_TIMEOUT ? RUNA_ERR_IO_TIMEOUT : RUNA_SPI_ERR_IO;
}

runa_status_t runa_esp32_spi_bus_initialize(spi_host_device_t host,
                                            const spi_bus_config_t *configuration) {
    spi_bus_config_t bus;
    esp_err_t status;
    if (configuration == NULL) return RUNA_ERR_INVALID_FORMAT;
    bus = *configuration;
    bus.max_transfer_sz = bus.max_transfer_sz > 0 && bus.max_transfer_sz < SOC_SPI_MAXIMUM_BUFFER_SIZE ?
                          bus.max_transfer_sz : SOC_SPI_MAXIMUM_BUFFER_SIZE;
    status = spi_bus_initialize(host, &bus, SPI_DMA_DISABLED);
    return status == ESP_OK ? RUNA_OK : map_spi_error(status);
}

runa_status_t runa_esp32_spi_device_add(spi_host_device_t host,
                                        const runa_spi_resource_config_t *configuration,
                                        spi_device_handle_t *device) {
    spi_device_interface_config_t interface_configuration = {0};
    esp_err_t status;
    int chip_select;
    if (configuration == NULL || device == NULL || configuration->mode > 3u ||
        configuration->clock_hz == 0u || configuration->clock_hz > (uint32_t)INT_MAX ||
        configuration->chip_select_handle >= (uintptr_t)GPIO_NUM_MAX ||
        configuration->chip_select_behavior > RUNA_SPI_CS_SOFTWARE)
        return RUNA_ERR_INVALID_FORMAT;
    chip_select = (int)configuration->chip_select_handle;
    if (configuration->chip_select_behavior == RUNA_SPI_CS_SOFTWARE) {
        gpio_config_t cs_output = {
            .pin_bit_mask = 1ULL << (uint32_t)chip_select,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        if (gpio_config(&cs_output) != ESP_OK || gpio_set_level((gpio_num_t)chip_select, 1u) != ESP_OK)
            return RUNA_SPI_ERR_IO;
        interface_configuration.spics_io_num = -1;
    } else {
        interface_configuration.spics_io_num = chip_select;
    }
    interface_configuration.clock_speed_hz = (int)configuration->clock_hz;
    interface_configuration.mode = configuration->mode;
    interface_configuration.queue_size = 1;
    status = spi_bus_add_device(host, &interface_configuration, device);
    return status == ESP_OK ? RUNA_OK : map_spi_error(status);
}

static runa_status_t transfer(void *context, uintptr_t device_handle,
                              const runa_spi_resource_config_t *configuration,
                              const uint8_t *transmit, size_t transmit_size,
                              uint8_t *receive, size_t receive_size,
                              uint16_t timeout_ms) {
    spi_device_handle_t device = (spi_device_handle_t)device_handle;
    size_t transfer_size = transmit_size > receive_size ? transmit_size : receive_size;
    uint64_t wire_time_us;
    uint64_t wire_time_ms;
    uint32_t acquisition_timeout_ms;
    TickType_t acquisition_timeout;
    esp_err_t status;
    size_t offset;
    size_t index;
    (void)context;
    if (device == NULL || configuration == NULL || timeout_ms == 0u ||
        transfer_size == 0u || transfer_size > RUNA_SPI_MAX_TRANSFER_BYTES ||
        (transmit_size != 0u && transmit == NULL) || (receive_size != 0u && receive == NULL) ||
        configuration->clock_hz == 0u)
        return RUNA_ERR_INVALID_FORMAT;

    if (transmit_size != 0u) memcpy(transmit_staging, transmit, transmit_size);
    for (index = transmit_size; index < transfer_size; ++index) transmit_staging[index] = 0u;
    wire_time_us = ((uint64_t)transfer_size * 8u * 1000000u + configuration->clock_hz - 1u) /
                   configuration->clock_hz;
    wire_time_ms = (wire_time_us + 999u) / 1000u;
    if ((uint64_t)timeout_ms <= wire_time_ms) return RUNA_ERR_OUT_OF_RANGE;
    acquisition_timeout_ms = (uint32_t)((uint64_t)timeout_ms - wire_time_ms);
    acquisition_timeout = pdMS_TO_TICKS(acquisition_timeout_ms);
    if (acquisition_timeout == 0) acquisition_timeout = 1;
    status = spi_device_acquire_bus(device, acquisition_timeout);
    if (status != ESP_OK) return map_spi_error(status);

    if (configuration->chip_select_behavior == RUNA_SPI_CS_SOFTWARE &&
        gpio_set_level((gpio_num_t)configuration->chip_select_handle, 0u) != ESP_OK) {
        spi_device_release_bus(device);
        return RUNA_SPI_ERR_IO;
    }
    status = ESP_OK;
    for (offset = 0u; offset < transfer_size; ) {
        spi_transaction_t transaction = {0};
        size_t chunk = transfer_size - offset;
        size_t received_chunk = receive_size > offset ? (size_t)receive_size - offset : 0u;
        if (chunk > SOC_SPI_MAXIMUM_BUFFER_SIZE) chunk = SOC_SPI_MAXIMUM_BUFFER_SIZE;
        if (received_chunk > chunk) received_chunk = chunk;
        transaction.length = chunk * 8u;
        transaction.rxlength = received_chunk * 8u;
        transaction.tx_buffer = transmit_staging + offset;
        transaction.rx_buffer = received_chunk != 0u ? receive_staging + offset : NULL;
        if (offset + chunk < transfer_size) transaction.flags |= SPI_TRANS_CS_KEEP_ACTIVE;
        status = spi_device_polling_transmit(device, &transaction);
        if (status != ESP_OK) break;
        offset += chunk;
    }
    if (configuration->chip_select_behavior == RUNA_SPI_CS_SOFTWARE &&
        gpio_set_level((gpio_num_t)configuration->chip_select_handle, 1u) != ESP_OK && status == ESP_OK)
        status = ESP_FAIL;
    spi_device_release_bus(device);
    if (status != ESP_OK) return map_spi_error(status);
    if (receive_size != 0u) memcpy(receive, receive_staging, receive_size);
    return RUNA_OK;
}

runa_spi_hal_t runa_esp32_spi_hal(void) {
    runa_spi_hal_t hal = { NULL, transfer };
    return hal;
}
