#include "runa_adc.h"
#include "runa_block_device.h"
#include "runa_can.h"
#include "runa_dac.h"
#include "runa_encoder.h"
#include "runa_gpio.h"
#include "runa_i2c.h"
#include "runa_limits.h"
#include "runa_onewire.h"
#include "runa_pulse.h"
#include "runa_pwm.h"
#include "runa_registry.h"
#include "runa_resource.h"
#include "runa_rtc.h"
#include "runa_spi.h"
#include "runa_uart.h"
#include "runa_validator.h"
#include "runa_watchdog.h"

#include <stdio.h>

int main(void) {
    uint8_t input[RUNA_MAX_JOB_BYTES];
    size_t size = fread(input, 1u, sizeof input, stdin);
    runa_module_registry_t registry;
    runa_resource_table_t resources = { NULL, 0u };
    runa_decoded_job_t decoded;
    runa_validation_error_t error;
    runa_module_t modules[] = {
        runa_gpio_module(NULL), runa_adc_module(NULL), runa_pwm_module(NULL),
        runa_spi_module(NULL), runa_i2c_module(NULL), runa_uart_module(NULL),
        runa_can_module(NULL), runa_pulse_module(NULL), runa_dac_module(NULL),
        runa_encoder_module(NULL), runa_onewire_module(NULL),
        runa_block_device_module(NULL), runa_rtc_module(NULL),
        runa_watchdog_module(NULL)
    };
    size_t index;
    runa_registry_init(&registry);
    for (index = 0u; index < sizeof modules / sizeof modules[0]; ++index)
        (void)runa_registry_add(&registry, &modules[index]);
    (void)runa_validate(input, size, &registry, &resources, &decoded, &error);
    return 0;
}
