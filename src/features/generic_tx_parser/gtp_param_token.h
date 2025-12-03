#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "common_utils.h"
#include "gtp_value.h"
#include "tlv.h"

#define MAX_NATIVE_ADDRS 4

typedef struct {
    uint8_t version;
    s_value address;
    uint8_t native_addr_count;
    uint8_t native_addrs[MAX_NATIVE_ADDRS][ADDRESS_LENGTH];
} s_param_token;

typedef struct {
    s_param_token *param;
} s_param_token_context;

bool handle_param_token_struct(const s_tlv_data *data, s_param_token_context *context);
bool format_param_token(const s_param_token *param, const char *name);
