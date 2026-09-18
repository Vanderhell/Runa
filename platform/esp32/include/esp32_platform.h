#ifndef RUNA_ESP32_PLATFORM_H
#define RUNA_ESP32_PLATFORM_H

#include <stddef.h>
#include <stdint.h>
#include "runa_core.h"
#include "runa_gpio.h"
#include "runa_adc.h"
#include "runa_pwm.h"

#define RUNA_REQUEST_HEADER_SIZE 8u
#define RUNA_REQUEST_MAX_SIZE (RUNA_REQUEST_HEADER_SIZE + RUNA_MAX_JOB_BYTES)
#define RUNA_COMMAND_GET_INFO 1u
#define RUNA_COMMAND_GET_CAPABILITIES_V1 2u
#define RUNA_COMMAND_EXECUTE 3u
#define RUNA_COMMAND_GET_CAPABILITIES_V2 4u
#define RUNA_EVENT_INFO 0x10u
#define RUNA_EVENT_CAPABILITIES_V1 0x11u
#define RUNA_EVENT_CAPABILITIES_V2 0x12u
#define RUNA_PLATFORM_ESP32S3 1u

extern const runa_resource_table_t runa_esp32_resources;
int runa_esp32_gpio_init(void);
runa_gpio_hal_t runa_esp32_gpio_hal(void);
int runa_esp32_adc_init(void);
runa_adc_hal_t runa_esp32_adc_hal(void);
int runa_esp32_pwm_init(void);
runa_pwm_hal_t runa_esp32_pwm_hal(void);
runa_platform_t runa_esp32_platform(void);

int runa_esp32_transport_init(void);
int runa_esp32_transport_read_request(uint8_t *command, uint8_t *payload,
                                      size_t capacity, size_t *payload_size);
int runa_esp32_transport_write(const uint8_t *data, size_t size);
void runa_esp32_send_info(void);
void runa_esp32_send_capabilities_v1(void);
void runa_esp32_send_capabilities_v2(const runa_module_registry_t *registry);

#endif
