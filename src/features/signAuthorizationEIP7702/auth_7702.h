#pragma once

#include <stdint.h>
#include "common_utils.h"
#include "tlv_library.h"

typedef struct {
    uint64_t chainId;
    uint64_t nonce;
    uint8_t delegate[ADDRESS_LENGTH];
} s_auth_7702;

typedef struct {
    s_auth_7702 auth_7702;
    TLV_reception_t received_tags;
} s_auth_7702_ctx;

bool handle_auth_7702_tlv_payload(const buffer_t *buf, s_auth_7702_ctx *context);
bool verify_auth_7702_struct(const s_auth_7702_ctx *context);
