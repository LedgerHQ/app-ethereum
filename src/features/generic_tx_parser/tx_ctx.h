#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "cx.h"
#include "list.h"
#include "common_utils.h"  // ADDRESS_LENGTH, INT256_LENGTH
#include "gtp_tx_info.h"
#include "calldata.h"
#include "gtp_field.h"

typedef struct {
    s_list_node _list;
    s_tx_info *tx_info;
    s_calldata *calldata;
    uint8_t from[ADDRESS_LENGTH];
    uint8_t to[ADDRESS_LENGTH];
    uint8_t amount[INT256_LENGTH];
    uint64_t chain_id;

    cx_sha3_t fields_hash_ctx;
} s_tx_ctx;

extern s_calldata *g_parked_calldata;

bool tx_ctx_is_root(void);
size_t get_tx_ctx_count(void);
cx_hash_t *get_fields_hash_ctx(void);
const s_tx_info *get_current_tx_info(void);
s_calldata *get_current_calldata(void);
const uint8_t *get_current_tx_from(void);
const uint8_t *get_current_tx_to(void);
const uint8_t *get_current_tx_amount(void);
bool validate_instruction_hash(void);
void tx_ctx_pop(void);
bool find_matching_tx_ctx(const uint8_t *contract_addr,
                          const uint8_t *selector,
                          const uint64_t *chain_id);
bool set_tx_info_into_tx_ctx(s_tx_info *tx_info);
bool tx_ctx_init(s_calldata *calldata,
                 const uint8_t *from,
                 const uint8_t *to,
                 const uint8_t *amount,
                 const uint64_t *chain_id);
void gcs_cleanup(void);
