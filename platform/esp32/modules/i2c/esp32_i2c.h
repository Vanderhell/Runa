#ifndef RUNA_ESP32_I2C_H
#define RUNA_ESP32_I2C_H

#include "driver/i2c_master.h"
#include "runa_i2c.h"

runa_status_t runa_esp32_i2c_bus_initialize(uint8_t bus, int sda_pin, int scl_pin);
runa_status_t runa_esp32_i2c_device_add(uint8_t bus, const runa_i2c_resource_config_t *configuration,
                                        i2c_master_dev_handle_t *device);
runa_i2c_hal_t runa_esp32_i2c_hal(void);

#endif
