#pragma once

#include <stdint.h>
#include "bip32_utils.h"
#include "common_utils.h"
#include "tlv.h"

typedef struct {
    bip32_path_t bip32;
    uint64_t chainId;
    uint64_t nonce;
    uint8_t delegate[20];
} s_auth_7702;

typedef struct {
    s_auth_7702 auth_7702;
    uint8_t mask_parsed;
} s_auth_7702_ctx;

bool handle_auth_7702_struct(const s_tlv_data *data, s_auth_7702_ctx *context);
bool verify_auth_7702_struct(s_auth_7702_ctx *context);
