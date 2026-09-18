#include "job_mock_hal.h"
#include <string.h>
static job_hal_status_t status(job_mock_hal_t *m){job_hal_status_t s=m->next_status;m->next_status=JOB_HAL_OK;m->calls++;return s;}
static job_hal_status_t gr(void *c,uint32_t h,uint32_t *v){job_mock_hal_t*m=c;job_hal_status_t s=status(m);if(h>=JOB_MOCK_CHANNELS||!v)return JOB_HAL_FAILURE;if(s==JOB_HAL_OK)*v=m->gpio[h];return s;}
static job_hal_status_t ar(void *c,uint32_t h,uint32_t *v){job_mock_hal_t*m=c;job_hal_status_t s=status(m);if(h>=JOB_MOCK_CHANNELS||!v)return JOB_HAL_FAILURE;if(s==JOB_HAL_OK)*v=m->adc[h];return s;}
static job_hal_status_t wr(void *c,uint32_t h,uint32_t v,uint8_t kind){job_mock_hal_t*m=c;job_hal_status_t s=status(m);if(h>=JOB_MOCK_CHANNELS)return JOB_HAL_FAILURE;if(s==JOB_HAL_OK&&m->write_count<JOB_MOCK_WRITES){m->writes[m->write_count].kind=kind;m->writes[m->write_count].handle=h;m->writes[m->write_count].value=v;m->write_count++;}return s;}
static job_hal_status_t gw(void*c,uint32_t h,uint32_t v){return wr(c,h,v,1u);}static job_hal_status_t pw(void*c,uint32_t h,uint32_t v){return wr(c,h,v,2u);}
static uint64_t tm(void*c){return ((job_mock_hal_t*)c)->now_us;}static job_hal_status_t dl(void*c,uint32_t ms){job_mock_hal_t*m=c;job_hal_status_t s=status(m);if(s==JOB_HAL_OK)m->now_us+=(uint64_t)ms*1000u;return s;}
void job_mock_hal_init(job_mock_hal_t*m){if(m)memset(m,0,sizeof *m);}job_hal_t job_mock_hal_interface(job_mock_hal_t*m){job_hal_t h={m,gr,gw,ar,pw,tm,dl};return h;}
