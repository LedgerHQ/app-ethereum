#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "cx.h"
#include "tlv.h"
#include "common_utils.h"  // ADDRESS_LENGTH
#include "calldata.h"      // CALLDATA_SELECTOR_SIZE
#include "signature.h"

typedef enum {
    DELEGATION_TYPE_PROXY = 0,
    DELEGATION_TYPE_ISSUED_FROM_FACTORY = 1,
    DELEGATION_TYPE_DELEGATOR = 2,
} e_delegation_type;

typedef struct {
    uint64_t chain_id;
    uint8_t address[ADDRESS_LENGTH];
    bool has_selector;
    uint8_t selector[CALLDATA_SELECTOR_SIZE];
    uint8_t implem_address[ADDRESS_LENGTH];
} s_proxy_info;

typedef struct {
    uint8_t struct_type;
    uint8_t version;
    e_delegation_type delegation_type;
    uint32_t challenge;
    s_proxy_info proxy_info;
    uint8_t signature_length;
    uint8_t signature[ECDSA_SIGNATURE_MAX_LENGTH];
    cx_sha256_t struct_hash;
} s_proxy_info_ctx;

bool handle_proxy_info_struct(const s_tlv_data *data, s_proxy_info_ctx *context);
bool verify_proxy_info_struct(const s_proxy_info_ctx *context);
const uint8_t *get_proxy_contract(const uint64_t *chain_id,
                                  const uint8_t *addr,
                                  const uint8_t *selector);
const uint8_t *get_implem_contract(const uint64_t *chain_id,
                                   const uint8_t *addr,
                                   const uint8_t *selector);
void proxy_cleanup(void);
