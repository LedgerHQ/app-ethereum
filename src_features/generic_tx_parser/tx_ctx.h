#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "cx.h"
#include "list.h"
#include "common_utils.h" // ADDRESS_LENGTH, INT256_LENGTH
#include "gtp_tx_info.h"
#include "calldata.h"
#include "gtp_field.h"

typedef struct {
    s_flist_node _list;
    const s_tx_info *tx_info;
    s_calldata *calldata;
    uint8_t from[ADDRESS_LENGTH];
    uint8_t to[ADDRESS_LENGTH];
    uint8_t amount[INT256_LENGTH];

    cx_sha3_t fields_hash_ctx;
    // only set for nested calldata
    s_field_list_node *fields;
} s_tx_ctx;

const s_tx_ctx *get_current_tx_ctx(void);
bool tx_ctx_is_root(void);
size_t get_tx_ctx_count(void);
cx_hash_t *get_fields_hash_ctx(void);
bool new_tx_ctx(const s_tx_info *tx_info);
