#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "buffer.h"
#include "gtp_value.h"

typedef struct {
    uint8_t version;
    s_value calldata;
    s_value contract_addr;
    bool has_chain_id;
    s_value chain_id;
    bool has_selector;
    s_value selector;
    bool has_amount;
    s_value amount;
    bool has_spender;
    s_value spender;
} s_param_calldata;

typedef struct {
    s_param_calldata *param;
} s_param_calldata_context;

bool handle_param_calldata_struct(const buffer_t *buf, s_param_calldata_context *context);
bool format_param_calldata(const s_param_calldata *param, const char *name);
