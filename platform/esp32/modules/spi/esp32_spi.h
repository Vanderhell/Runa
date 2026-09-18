#ifndef RUNA_ESP32_SPI_H
#define RUNA_ESP32_SPI_H

#include "driver/spi_master.h"
#include "runa_spi.h"

/* Resources store the spi_device_handle_t returned by device_add as their
 * opaque platform_handle. The Core and generic SPI module never see IDF types. */
runa_status_t runa_esp32_spi_bus_initialize(spi_host_device_t host,
                                            const spi_bus_config_t *configuration);
runa_status_t runa_esp32_spi_device_add(spi_host_device_t host,
                                        const runa_spi_resource_config_t *configuration,
                                        spi_device_handle_t *device);
runa_spi_hal_t runa_esp32_spi_hal(void);

#endif
