#include "runa_adc.h"
#include "runa_block_device.h"
#include "runa_can.h"
#include "runa_capabilities.h"
#include "runa_dac.h"
#include "runa_encoder.h"
#include "runa_gpio.h"
#include "runa_i2c.h"
#include "runa_ir.h"
#include "runa_module.h"
#include "runa_onewire.h"
#include "runa_pulse.h"
#include "runa_pwm.h"
#include "runa_registry.h"
#include "runa_result.h"
#include "runa_rtc.h"
#include "runa_spi.h"
#include "runa_uart.h"
#include "runa_watchdog.h"

_Static_assert(RUNA_MODULE_ABI_VERSION == 1u, "module ABI changed");
_Static_assert(RUNA_IR_VERSION_V1 == 1u && RUNA_IR_VERSION_V2 == 2u, "IR versions changed");
_Static_assert(RUNA_HEADER_SIZE == 40u, "job header wire size changed");
_Static_assert(RUNA_CAPABILITIES_FORMAT_VERSION == 2u && RUNA_CAPABILITIES_HEADER_SIZE == 32u,
               "capability wire format changed");
_Static_assert(RUNA_MAX_MODULES == 24u, "registry capacity changed");
_Static_assert(RUNA_MAX_MODULE_DATA_BYTES == 48u, "module-data payload limit changed");
_Static_assert(RUNA_EVENT_ACK == 1 && RUNA_EVENT_EMIT == 2 &&
               RUNA_EVENT_RESULT == 3 && RUNA_EVENT_MODULE_DATA == 4,
               "event type changed");

_Static_assert(RUNA_GPIO_MODULE_ID == 1u && RUNA_ADC_MODULE_ID == 2u &&
               RUNA_PWM_MODULE_ID == 3u && RUNA_SPI_MODULE_ID == 4u &&
               RUNA_I2C_MODULE_ID == 5u && RUNA_UART_MODULE_ID == 6u &&
               RUNA_CAN_MODULE_ID == 7u && RUNA_PULSE_MODULE_ID == 8u &&
               RUNA_DAC_MODULE_ID == 9u && RUNA_ENCODER_MODULE_ID == 10u &&
               RUNA_ONEWIRE_MODULE_ID == 11u && RUNA_BLOCK_DEVICE_MODULE_ID == 12u &&
               RUNA_RTC_MODULE_ID == 13u && RUNA_WATCHDOG_MODULE_ID == 14u,
               "module ID freeze changed");

_Static_assert(RUNA_GPIO_OP_READ == 1u && RUNA_GPIO_OP_WRITE == 2u &&
               RUNA_ADC_OP_READ == 1u && RUNA_PWM_OP_WRITE == 1u &&
               RUNA_SPI_OP_TRANSFER == 1u && RUNA_I2C_OP_TRANSFER == 1u &&
               RUNA_UART_OP_TRANSFER == 1u && RUNA_CAN_OP_TRANSMIT == 1u &&
               RUNA_CAN_OP_RECEIVE == 2u && RUNA_CAN_OP_REQUEST_RESPONSE == 3u &&
               RUNA_PULSE_OP_COUNT == 1u && RUNA_PULSE_OP_MEASURE_WIDTH == 2u &&
               RUNA_PULSE_OP_MEASURE_PERIOD == 3u && RUNA_DAC_OP_WRITE == 1u &&
               RUNA_ENCODER_OP_READ == 1u && RUNA_ENCODER_OP_RESET == 2u &&
               RUNA_ENCODER_OP_READ_RESET == 3u && RUNA_ONEWIRE_OP_RESET == 1u &&
               RUNA_ONEWIRE_OP_TRANSFER == 2u && RUNA_ONEWIRE_OP_ROM_SEARCH == 3u,
               "module operation freeze changed");

int main(void) { return 0; }
