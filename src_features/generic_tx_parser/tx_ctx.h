#pragma once

#include <stdint.h>
#include "list.h"
#include "common_utils.h" // ADDRESS_LENGTH, INT256_LENGTH
#include "gtp_tx_info.h"
#include "calldata.h"

typedef struct {
    s_flist_node _list;
    s_tx_info *tx_info;
    s_calldata *calldata;
    uint8_t from[ADDRESS_LENGTH];
    uint8_t to[ADDRESS_LENGTH];
    uint8_t amount[INT256_LENGTH];
} s_tx_ctx;

const s_tx_ctx *get_current_tx_ctx(void);
