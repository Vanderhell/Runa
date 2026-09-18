#ifndef JOB_VALIDATOR_H
#define JOB_VALIDATOR_H

#include "job_error.h"
#include "job_ir.h"
#include "job_resources.h"

typedef struct job_validation_error {
    job_error_t code;
    uint16_t instruction_index;
    uint32_t detail;
} job_validation_error_t;

job_error_t job_decode_header(const uint8_t *data, size_t size, job_header_t *header);
job_error_t job_decode_instruction(const uint8_t *data, size_t size, uint32_t offset,
                                   job_instruction_t *instruction);
job_error_t job_validate(const uint8_t *data, size_t size,
                         const job_resource_table_t *resources,
                         job_header_t *header, job_validation_error_t *error);

#endif

