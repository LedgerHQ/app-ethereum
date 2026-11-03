#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "cx.h"
#include "lists.h"
#include "common_utils.h"  // ADDRESS_LENGTH, INT256_LENGTH
#include "gtp_tx_info.h"
#include "calldata.h"
#include "gcs_field.h"

typedef struct {
    list_node_t _list;
    s_tx_info *tx_info;
    s_calldata *calldata;
    uint8_t from[ADDRESS_LENGTH];
    uint8_t to[ADDRESS_LENGTH];
    uint8_t amount[INT256_LENGTH];
    uint64_t chain_id;

    cx_sha3_t fields_hash_ctx;
    bool has_amount : 1;
} s_operation_ctx;

extern s_calldata *g_parked_calldata;

bool operation_ctx_is_root(void);
size_t get_operation_ctx_count(void);
cx_hash_t *get_fields_hash_ctx(void);
const s_tx_info *get_root_operation_info(void);
const s_tx_info *get_current_tx_info(void);
s_calldata *get_current_calldata(void);
s_calldata *get_root_calldata(void);
const uint8_t *get_current_tx_from(void);
const uint8_t *get_current_tx_to(void);
const uint8_t *get_current_tx_amount(void);
uint64_t get_current_tx_chain_id(void);
bool validate_instruction_hash(void);
void operation_ctx_pop(void);
bool process_empty_calls_before(void);
bool process_empty_calls_after(void);
bool find_matching_operation_ctx(const uint8_t *contract_addr,
                                 const uint8_t *selector,
                                 const uint64_t *chain_id);
bool operation_ctx_set_info(s_tx_info *tx_info);
bool operation_ctx_init(s_calldata *calldata,
                        const uint8_t *from,
                        const uint8_t *to,
                        const uint8_t *amount,
                        const uint64_t *chain_id);
void gcs_cleanup(void);
void operation_ctx_set_root_value_shown(void);
bool operation_ctx_is_root_value_shown(void);
void operation_ctx_set_root_from_shown(void);
bool operation_ctx_is_root_from_shown(void);
