#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "cx.h"
#include "common_utils.h"  // ADDRESS_LENGTH, INT256_LENGTH
#include "calldata.h"
#include "tlv.h"
#include "signature.h"
#include "lists.h"
#include "gtp_field.h"

typedef struct {
    uint8_t version;
    uint64_t chain_id;
    uint8_t contract_addr[ADDRESS_LENGTH];
    uint8_t selector[CALLDATA_SELECTOR_SIZE];
    uint8_t fields_hash[INT256_LENGTH];
    char operation_type[31];
    char creator_name[23];
    char creator_legal_name[31];
    char creator_url[27];
    char contract_name[31];
    // YYYY-MM-DD\0
    char deploy_date[4 + 1 + 2 + 1 + 2 + 1];
    uint8_t signature_len;
    uint8_t signature[ECDSA_SIGNATURE_MAX_LENGTH];
} s_tx_info;

typedef struct {
    cx_sha256_t struct_hash;
    uint16_t set_flags;
    s_tx_info *tx_info;
} s_tx_info_ctx;

bool handle_tx_info_struct(const s_tlv_data *data, s_tx_info_ctx *context);
bool verify_tx_info_struct(const s_tx_info_ctx *context);

const char *get_operation_type(const s_tx_info *tx_info);
const char *get_creator_name(const s_tx_info *tx_info);
const char *get_creator_legal_name(const s_tx_info *tx_info);
const char *get_creator_url(const s_tx_info *tx_info);
const char *get_contract_name(const s_tx_info *tx_info);
const uint8_t *get_contract_addr(const s_tx_info *tx_info);
const char *get_deploy_date(const s_tx_info *tx_info);
void delete_tx_info(s_tx_info *node);
