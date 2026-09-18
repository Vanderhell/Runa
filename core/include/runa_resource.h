#ifndef RUNA_RESOURCE_H
#define RUNA_RESOURCE_H

#include <stddef.h>
#include <stdint.h>
#include "runa_error.h"

struct runa_module_registry;

#define RUNA_MAX_RESOURCES 64u

enum { RUNA_PERMISSION_READ = 1u, RUNA_PERMISSION_WRITE = 2u };

typedef struct runa_resource {
    uint16_t id;
    uint16_t module_id;
    uint16_t resource_type;
    uint16_t reserved;
    uint32_t permissions;
    uintptr_t platform_handle;
    const void *config;
} runa_resource_t;

typedef struct runa_resource_table {
    const runa_resource_t *items;
    size_t count;
} runa_resource_table_t;

const runa_resource_t *runa_resource_find(const runa_resource_table_t *table, uint16_t id);
runa_status_t runa_resource_table_validate(const runa_resource_table_t *table,
                                           const struct runa_module_registry *registry);

#endif
