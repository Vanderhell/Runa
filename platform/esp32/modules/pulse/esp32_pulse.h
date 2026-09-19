#ifndef RUNA_ESP32_PULSE_H
#define RUNA_ESP32_PULSE_H

#include "runa_pulse.h"

int runa_esp32_pulse_init(int gpio_num);
runa_pulse_hal_t runa_esp32_pulse_hal(void);

#endif
