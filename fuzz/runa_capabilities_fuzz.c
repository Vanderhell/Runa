#include "runa_capabilities.h"
#include "runa_limits.h"

#include <stdio.h>

int main(void) {
    uint8_t input[RUNA_MAX_CAPABILITY_BYTES];
    runa_capabilities_view_t view;
    size_t size = fread(input, 1u, sizeof input, stdin);
    (void)runa_capabilities_decode(input, size, &view);
    return 0;
}
