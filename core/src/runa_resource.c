#include "runa_resource.h"
#include "runa_registry.h"

const runa_resource_t *runa_resource_find(const runa_resource_table_t *table, uint16_t id) {
    size_t index;
    if (table == NULL || table->count > RUNA_MAX_RESOURCES ||
        (table->count != 0u && table->items == NULL)) return NULL;
    for (index = 0u; index < table->count; ++index) {
        if (table->items[index].id == id) return &table->items[index];
    }
    return NULL;
}

runa_status_t runa_resource_table_validate(const runa_resource_table_t *table,
                                        const runa_module_registry_t *registry) {
    size_t index;
    size_t other;
    if (runa_registry_validate(registry) != RUNA_OK) return RUNA_ERR_INVALID_FORMAT;
    if (table == NULL || table->count > RUNA_MAX_RESOURCES ||
        (table->count != 0u && table->items == NULL)) return RUNA_ERR_INVALID_FORMAT;
    for (index = 0u; index < table->count; ++index) {
        const runa_resource_t *resource = &table->items[index];
        const runa_module_t *module = runa_registry_find(registry, resource->module_id);
        uint32_t detail = resource->id;
        runa_status_t status;
        if (resource->module_id == 0u || module == NULL)
            return RUNA_ERR_INVALID_RESOURCE;
        if ((resource->permissions & ~(uint32_t)(RUNA_PERMISSION_READ | RUNA_PERMISSION_WRITE)) != 0u)
            return RUNA_ERR_INVALID_RESOURCE;
        status = module->validate_resource(module->context, resource, &detail);
        if (status != RUNA_OK) return status;
        for (other = index + 1u; other < table->count; ++other) {
            if (resource->id == table->items[other].id) return RUNA_ERR_INVALID_RESOURCE;
        }
    }
    return RUNA_OK;
}
