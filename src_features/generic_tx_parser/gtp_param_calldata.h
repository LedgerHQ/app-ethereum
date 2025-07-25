#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "tlv.h"
#include "gtp_value.h"
#include "list.h"

typedef struct {
    uint8_t version;
    s_value calldata;
    s_value contract_addr;
    // TODO: add from / value
    bool has_chain_id;
    s_value chain_id;
    bool has_selector;
    s_value selector;
} s_param_calldata;

typedef struct {
    s_param_calldata *param;
} s_param_calldata_context;

typedef struct {
    s_flist_node _list;
    s_param_calldata value;
} s_calldata_list;

extern s_calldata_list *g_calldata;

bool handle_param_calldata_struct(const s_tlv_data *data, s_param_calldata_context *context);
bool format_param_calldata(const s_param_calldata *param, const char *name);
