#pragma once

#include <stdint.h>
#include "common_utils.h"
#include "asset_info.h"

typedef struct {
    uint8_t address[ADDRESS_LENGTH];
    char collection_name[COLLECTION_NAME_MAX_LEN + 1];

    uint64_t chain_id;
} s_nft_info;

void clear_nft_infos(void);
