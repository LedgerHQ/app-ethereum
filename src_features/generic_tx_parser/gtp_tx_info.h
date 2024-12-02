#ifndef GTP_TX_INFO_H_
#define GTP_TX_INFO_H_

#include <stdbool.h>
#include <stdint.h>
#include "cx.h"
#include "common_utils.h"  // ADDRESS_LENGTH, INT256_LENGTH
#include "calldata.h"
#include "tlv.h"

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
    uint8_t signature[73];
} s_tx_info;

typedef struct {
    cx_sha256_t struct_hash;
    uint16_t set_flags;
    s_tx_info *tx_info;
} s_tx_info_ctx;

extern s_tx_info *g_tx_info;

bool handle_tx_info_struct(const s_tlv_data *data, s_tx_info_ctx *context);
bool verify_tx_info_struct(const s_tx_info_ctx *context);

const char *get_operation_type(void);
const char *get_creator_name(void);
const char *get_creator_legal_name(void);
const char *get_creator_url(void);
const char *get_contract_name(void);
const uint8_t *get_contract_addr(void);
const char *get_deploy_date(void);
cx_hash_t *get_fields_hash_ctx(void);
bool validate_instruction_hash(void);

#endif  // !GTP_TX_INFO_H_
