#pragma once

#include <stdint.h>
#include "nbgl_types.h"

typedef struct {
    uint64_t chain_id;
    const nbgl_icon_details_t *icon;
} network_icon_t;

// Stub: chain_id=0 never matches a real chain, so the table is effectively empty.
extern const network_icon_t g_network_icons[1];
