#ifndef JOB_RESOURCES_H
#define JOB_RESOURCES_H

#include <stddef.h>
#include <stdint.h>

typedef enum job_resource_type {
    JOB_RESOURCE_GPIO = 1,
    JOB_RESOURCE_ADC = 2,
    JOB_RESOURCE_PWM = 3
} job_resource_type_t;

enum { JOB_PERMISSION_READ = 1u, JOB_PERMISSION_WRITE = 2u };

typedef struct job_resource {
    uint16_t id;
    uint8_t type;
    uint8_t permissions;
    uint32_t platform_handle;
    uint32_t maximum_value;
} job_resource_t;

typedef struct job_resource_table {
    const job_resource_t *items;
    size_t count;
} job_resource_table_t;

const job_resource_t *job_resource_find(const job_resource_table_t *table, uint16_t id);

#endif

