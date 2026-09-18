#include "esp32_platform.h"

#include "esp_adc/adc_oneshot.h"
#include "soc/soc_caps.h"

typedef struct esp32_adc_context {
    adc_oneshot_unit_handle_t unit;
} esp32_adc_context_t;

static esp32_adc_context_t adc_context;

static runa_status_t read_adc(void *context, uintptr_t handle, uint32_t *value) {
    esp32_adc_context_t *state = (esp32_adc_context_t *)context;
    int raw;
    if (state == NULL || state->unit == NULL || value == NULL ||
        handle >= SOC_ADC_MAX_CHANNEL_NUM) return RUNA_ADC_ERR_IO;
    if (adc_oneshot_read(state->unit, (adc_channel_t)handle, &raw) != ESP_OK || raw < 0)
        return RUNA_ADC_ERR_IO;
    *value = (uint32_t)raw;
    return RUNA_OK;
}

int runa_esp32_adc_init(void) {
    adc_oneshot_unit_init_cfg_t unit = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE
    };
    adc_oneshot_chan_cfg_t channel = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12
    };
    if (adc_oneshot_new_unit(&unit, &adc_context.unit) != ESP_OK) return -1;
    if (adc_oneshot_config_channel(adc_context.unit, ADC_CHANNEL_0, &channel) != ESP_OK) return -1;
    return 0;
}

runa_adc_hal_t runa_esp32_adc_hal(void) {
    runa_adc_hal_t hal = { &adc_context, read_adc };
    return hal;
}
