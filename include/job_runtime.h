#ifndef JOB_RUNTIME_H
#define JOB_RUNTIME_H
#include "job_hal.h"
#include "job_result.h"
#include "job_resources.h"
#include <stddef.h>
#include <stdint.h>
typedef struct {
    job_error_t error;
    uint32_t steps;
    uint32_t emit_count;
    uint32_t emit_bytes;
    uint8_t accepted;
    uint8_t result_sent;
} job_execution_summary_t;
job_execution_summary_t job_process(const uint8_t *, size_t, const job_resource_table_t *,
                                    const job_hal_t *, const job_event_sink_t *);
#endif
