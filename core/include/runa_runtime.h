#ifndef RUNA_RUNTIME_H
#define RUNA_RUNTIME_H

#include "runa_platform.h"
#include "runa_registry.h"
#include "runa_resource.h"
#include "runa_result.h"

typedef struct runa_execution_summary {
    runa_status_t error;
    uint32_t steps;
    uint32_t emit_count;
    uint32_t emit_bytes;
    uint8_t accepted;
    uint8_t result_sent;
} runa_execution_summary_t;

runa_execution_summary_t runa_process(const uint8_t *data, size_t size,
                                      const runa_resource_table_t *resources,
                                      const runa_module_registry_t *registry,
                                      const runa_platform_t *platform,
                                      const runa_event_sink_t *sink);

#endif
