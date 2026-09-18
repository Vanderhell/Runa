#ifndef JOB_RESULT_H
#define JOB_RESULT_H
#include "job_error.h"
#include <stddef.h>
#include <stdint.h>
typedef enum { JOB_EVENT_ACK = 1, JOB_EVENT_EMIT = 2, JOB_EVENT_RESULT = 3 } job_event_type_t;
typedef enum { JOB_RESULT_OK = 0, JOB_RESULT_ERROR = 1 } job_result_status_t;
typedef int (*job_event_sink_fn)(void *, const uint8_t *, size_t);
typedef struct { job_event_sink_fn send; void *context; } job_event_sink_t;
size_t job_encode_ack(uint8_t *, size_t, uint32_t);
size_t job_encode_emit(uint8_t *, size_t, uint32_t, uint16_t, uint32_t);
size_t job_encode_result(uint8_t *, size_t, uint32_t, job_result_status_t, job_error_t,
                         uint16_t, uint32_t, const uint8_t *, uint16_t);
#endif
