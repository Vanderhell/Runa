#ifndef RUNA_RTC_MOCK_HAL_H
#define RUNA_RTC_MOCK_HAL_H
#include "runa_rtc.h"
typedef struct runa_rtc_mock_hal { runa_rtc_time_t current_time; uint32_t status_flags; uint8_t set_supported; runa_status_t next_read_status; runa_status_t next_set_status; runa_status_t next_status_status; uint32_t read_calls; uint32_t set_calls; uint32_t status_calls; runa_rtc_time_t last_set_time; } runa_rtc_mock_hal_t;
void runa_rtc_mock_hal_init(runa_rtc_mock_hal_t *mock);
runa_rtc_hal_t runa_rtc_mock_hal_interface(runa_rtc_mock_hal_t *mock);
#endif
