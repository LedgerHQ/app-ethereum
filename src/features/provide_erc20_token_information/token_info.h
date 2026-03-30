#pragma once

#include <stdint.h>
#include "common_utils.h"
#include "asset_info.h"

typedef struct {
    uint8_t address[ADDRESS_LENGTH];
    char ticker[MAX_TICKER_LEN];
    uint8_t decimals;

    uint64_t chain_id;
} s_token_info;

void clear_token_infos(void);
