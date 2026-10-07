#pragma once

#include <stdint.h>
#include "shared_context.h"

extraInfo_t *get_matching_asset_info(const uint64_t *chain_id, const uint8_t *address);
void forget_known_assets(void);
