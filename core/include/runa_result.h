#ifndef RUNA_RESULT_H
#define RUNA_RESULT_H

#include <stddef.h>
#include <stdint.h>
#include "runa_error.h"

typedef enum runa_event_type {
    RUNA_EVENT_ACK = 1, RUNA_EVENT_EMIT = 2, RUNA_EVENT_RESULT = 3,
    RUNA_EVENT_MODULE_DATA = 4
} runa_event_type_t;
#define RUNA_MAX_MODULE_DATA_BYTES 48u
/* Module data shares the job-wide emit count and byte budgets. */
typedef enum runa_result_status { RUNA_RESULT_OK = 0, RUNA_RESULT_ERROR = 1 } runa_result_status_t;
typedef int (*runa_event_sink_fn)(void *context, const uint8_t *data, size_t size);
typedef struct runa_event_sink { runa_event_sink_fn send; void *context; } runa_event_sink_t;

size_t runa_encode_ack(uint8_t *output, size_t capacity, uint32_t job_id);
size_t runa_encode_emit(uint8_t *output, size_t capacity, uint32_t job_id,
                        uint16_t instruction, uint32_t value);
size_t runa_encode_result(uint8_t *output, size_t capacity, uint32_t job_id,
                          runa_result_status_t result, runa_status_t status,
                          uint16_t instruction, uint32_t detail,
                          const uint8_t *payload, uint16_t payload_size);
size_t runa_encode_module_data(uint8_t *output, size_t capacity, uint32_t job_id,
                               uint16_t module_id, uint16_t instruction,
                               uint16_t sequence, const uint8_t *payload,
                               uint16_t payload_size);

#endif
