#include "esp32_platform.h"

#include "esp_rom_sys.h"
#include "esp_timer.h"

static uint64_t monotonic_time_us(void *context) {
    (void)context;
    return (uint64_t)esp_timer_get_time();
}

static runa_status_t bounded_delay_ms(void *context, uint32_t milliseconds) {
    (void)context;
    while (milliseconds != 0u) {
        uint32_t chunk = milliseconds > 10u ? 10u : milliseconds;
        esp_rom_delay_us(chunk * 1000u);
        milliseconds -= chunk;
    }
    return RUNA_OK;
}

runa_platform_t runa_esp32_platform(void) {
    runa_platform_t platform = { NULL, monotonic_time_us, bounded_delay_ms };
    return platform;
}
