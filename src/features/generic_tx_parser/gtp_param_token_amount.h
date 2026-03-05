#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "uint256.h"
#include "buffer.h"
#include "gtp_value.h"
#include "common_utils.h"

#define MAX_NATIVE_ADDRS 4

typedef struct {
    uint8_t version;
    s_value value;
    bool has_token;
    s_value token;
    uint8_t native_addr_count;
    uint8_t native_addrs[MAX_NATIVE_ADDRS][ADDRESS_LENGTH];
    uint256_t threshold;
    char above_threshold_msg[21];
} s_param_token_amount;

typedef struct {
    s_param_token_amount *param;
} s_param_token_amount_context;

bool handle_param_token_amount_struct(const buffer_t *buf, s_param_token_amount_context *context);
bool format_param_token_amount(const s_param_token_amount *param, const char *name);
