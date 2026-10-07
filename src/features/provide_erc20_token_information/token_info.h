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

int set_token_info(const s_token_info *info);
void clear_token_infos(void);
const s_token_info *get_matching_token_info(const uint64_t *chain_id, const uint8_t *address);
const s_token_info *get_matching_token_info_or_dummy(const uint64_t *chain_id,
                                                     const uint8_t *address);
