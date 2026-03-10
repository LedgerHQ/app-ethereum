#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "cx.h"
#include "common_utils.h"  // ADDRESS_LENGTH, INT256_LENGTH
#include "calldata.h"
#include "buffer.h"
#include "lcx_ecdsa.h"
#include "lists.h"
#include "gtp_field.h"
#include "lcx_ecdsa.h"

#define OPERATION_TYPE_SIZE     31
#define CREATOR_NAME_SIZE       23
#define CREATOR_LEGAL_NAME_SIZE 31
#define CREATOR_URL_SIZE        27
#define CONTRACT_NAME_SIZE      31
#define DEPLOY_DATE_SIZE        11  // "YYYY-MM-DD\0" -> 4 + 1 + 2 + 1 + 2 + 1

typedef struct {
    uint8_t version;
    uint64_t chain_id;
    uint8_t contract_addr[ADDRESS_LENGTH];
    uint8_t selector[CALLDATA_SELECTOR_SIZE];
    uint8_t fields_hash[INT256_LENGTH];
    char operation_type[OPERATION_TYPE_SIZE];
    char creator_name[CREATOR_NAME_SIZE];
    char creator_legal_name[CREATOR_LEGAL_NAME_SIZE];
    char creator_url[CREATOR_URL_SIZE];
    char contract_name[CONTRACT_NAME_SIZE];
    char deploy_date[DEPLOY_DATE_SIZE];
    uint8_t signature_len;
    uint8_t signature[CX_ECDSA_SHA256_SIG_MAX_ASN1_LENGTH];
} s_tx_info;

typedef struct {
    cx_sha256_t struct_hash;
    s_tx_info *tx_info;
    TLV_reception_t received_tags;
} s_tx_info_ctx;

bool handle_tx_info_struct(const buffer_t *buf, s_tx_info_ctx *context);
bool verify_tx_info_struct(const s_tx_info_ctx *context);

const char *get_operation_type(const s_tx_info *tx_info);
const char *get_creator_name(const s_tx_info *tx_info);
const char *get_creator_legal_name(const s_tx_info *tx_info);
const char *get_creator_url(const s_tx_info *tx_info);
const char *get_contract_name(const s_tx_info *tx_info);
const uint8_t *get_contract_addr(const s_tx_info *tx_info);
const char *get_deploy_date(const s_tx_info *tx_info);
void delete_tx_info(s_tx_info *node);
