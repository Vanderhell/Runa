#ifndef JOB_MOCK_HAL_H
#define JOB_MOCK_HAL_H
#include "job_hal.h"
#include <stdint.h>
#define JOB_MOCK_CHANNELS 32u
#define JOB_MOCK_WRITES 256u
typedef struct {uint8_t kind;uint32_t handle;uint32_t value;} job_mock_write_t;
typedef struct {
 uint32_t gpio[JOB_MOCK_CHANNELS],adc[JOB_MOCK_CHANNELS];uint64_t now_us;
 job_hal_status_t next_status;job_mock_write_t writes[JOB_MOCK_WRITES];uint32_t write_count;uint32_t calls;
} job_mock_hal_t;
void job_mock_hal_init(job_mock_hal_t *);
job_hal_t job_mock_hal_interface(job_mock_hal_t *);
#endif
