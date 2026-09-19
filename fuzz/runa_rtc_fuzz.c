#include "runa_rtc.h"
#include "runa_limits.h"
#include "runa_registry.h"
#include "runa_runtime.h"
#include "runa_rtc_mock_hal.h"
#include <stdio.h>
static int discard(void *context, const uint8_t *data, size_t size) { (void)context; (void)data; (void)size; return 0; }
int main(void) { uint8_t input[RUNA_MAX_JOB_BYTES]; size_t size=fread(input,1u,sizeof input,stdin); runa_rtc_mock_hal_t mock; runa_rtc_hal_t hal; runa_module_t module; runa_module_registry_t registry; runa_rtc_resource_config_t config={0u,RUNA_RTC_MAX_SECONDS,RUNA_RTC_CAP_SET_SUPPORTED,RUNA_RTC_RUNTIME_STATUS_MASK}; runa_resource_t resource={13u,13u,1u,0u,RUNA_PERMISSION_READ|RUNA_PERMISSION_WRITE,1u,&config}; runa_resource_table_t resources={&resource,1u}; runa_platform_t platform={NULL,NULL,NULL}; runa_event_sink_t sink={discard,NULL}; if (size==0u) return 0; runa_rtc_mock_hal_init(&mock); hal=runa_rtc_mock_hal_interface(&mock); module=runa_rtc_module(&hal); runa_registry_init(&registry); if (runa_registry_add(&registry,&module)==RUNA_OK) (void)runa_process(input,size,&resources,&registry,&platform,&sink); return 0; }
