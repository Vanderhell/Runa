#include "runa_capabilities.h"
#include "runa_limits.h"
#include "runa_registry.h"
#include "runa_resource.h"
#include "runa_validator.h"

#include "runa_adc.h"
#include "runa_block_device.h"
#include "runa_can.h"
#include "runa_dac.h"
#include "runa_encoder.h"
#include "runa_gpio.h"
#include "runa_i2c.h"
#include "runa_onewire.h"
#include "runa_pulse.h"
#include "runa_pwm.h"
#include "runa_rtc.h"
#include "runa_spi.h"
#include "runa_uart.h"
#include "runa_watchdog.h"

#include <stddef.h>
#include <stdint.h>

static runa_module_registry_t make_registry(void) {
    runa_module_registry_t registry;
    static runa_module_t modules[] = {
        {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}
    };
    modules[0] = runa_gpio_module(NULL);
    modules[1] = runa_adc_module(NULL);
    modules[2] = runa_pwm_module(NULL);
    modules[3] = runa_spi_module(NULL);
    modules[4] = runa_i2c_module(NULL);
    modules[5] = runa_uart_module(NULL);
    modules[6] = runa_can_module(NULL);
    modules[7] = runa_pulse_module(NULL);
    modules[8] = runa_dac_module(NULL);
    modules[9] = runa_encoder_module(NULL);
    modules[10] = runa_onewire_module(NULL);
    modules[11] = runa_block_device_module(NULL);
    modules[12] = runa_rtc_module(NULL);
    modules[13] = runa_watchdog_module(NULL);
    runa_registry_init(&registry);
    for (size_t index = 0u; index < sizeof modules / sizeof modules[0]; ++index)
        (void)runa_registry_add(&registry, &modules[index]);
    return registry;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint8_t job[RUNA_MAX_JOB_BYTES];
    runa_capabilities_view_t capabilities;
    runa_decoded_job_t decoded;
    runa_validation_error_t error;
    if (size != 0u && (data[0] & 1u) != 0u) {
        (void)runa_capabilities_decode(data, size, &capabilities);
    } else {
        runa_module_registry_t registry = make_registry();
        const runa_resource_table_t resources = { NULL, 0u };
        if (size < 2u) return 0;
        size_t payload_size = size > 2u ? size - 2u : 0u;
        if (payload_size > 32u) payload_size = 32u;
        size_t instruction_size = 2u + 3u + payload_size + 2u + 1u;
        size_t total_size = RUNA_HEADER_SIZE + instruction_size;
        if (total_size <= sizeof job) {
            size_t index;
            for (index = 0u; index < total_size; ++index) job[index] = 0u;
            job[0] = (uint8_t)'J'; job[1] = (uint8_t)'E';
            job[2] = (uint8_t)'X'; job[3] = (uint8_t)'E';
            job[4] = RUNA_PROTOCOL_VERSION; job[5] = RUNA_IR_VERSION_V2;
            runa_write_u16_le(job + 6u, RUNA_HEADER_SIZE);
            runa_write_u32_le(job + 8u, 1u);
            runa_write_u32_le(job + 12u, (uint32_t)total_size);
            runa_write_u32_le(job + 16u, (uint32_t)instruction_size);
            runa_write_u16_le(job + 20u, 2u);
            runa_write_u32_le(job + 24u, RUNA_MAX_STEPS);
            runa_write_u32_le(job + 28u, RUNA_MAX_RUNTIME_US);
            runa_write_u16_le(job + 32u, RUNA_MAX_RESULT_BYTES);
            runa_write_u16_le(job + 34u, RUNA_MAX_EMITS);
            runa_write_u32_le(job + 36u, RUNA_MAX_EMIT_BYTES);
            job[40] = RUNA_OP_EXT;
            job[41] = (uint8_t)(3u + payload_size);
            runa_write_u16_le(job + 42u, (uint16_t)((data[0] % 14u) + 1u));
            job[44] = (uint8_t)((data[1] % 4u) + 1u);
            for (index = 0u; index < payload_size; ++index) job[45u + index] = data[index + 2u];
            job[45u + payload_size] = RUNA_OP_RETURN;
            job[46u + payload_size] = 1u;
            job[47u + payload_size] = 0u;
            (void)runa_validate(job, total_size, &registry, &resources, &decoded, &error);
        }
    }
    return 0;
}
