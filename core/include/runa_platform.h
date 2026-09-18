#ifndef RUNA_PLATFORM_H
#define RUNA_PLATFORM_H

#include <stdint.h>
#include "runa_error.h"

typedef struct runa_platform {
    void *context;
    uint64_t (*time_us)(void *context);
    runa_status_t (*delay_ms)(void *context, uint32_t milliseconds);
} runa_platform_t;

#endif
