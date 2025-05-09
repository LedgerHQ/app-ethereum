#if defined(HAVE_EIP712_FULL_SUPPORT) || defined(HAVE_GENERIC_TX_PARSER)

#ifndef PROXY_INFO_H_
#define PROXY_INFO_H_

#include <stdint.h>
#include <stdbool.h>
#include "cx.h"
#include "tlv.h"
#include "common_utils.h"  // ADDRESS_LENGTH
#include "calldata.h"      // CALLDATA_SELECTOR_SIZE
#include "signature.h"

typedef struct {
    uint64_t chain_id;
    uint8_t address[ADDRESS_LENGTH];
    bool has_selector;
    uint8_t selector[CALLDATA_SELECTOR_SIZE];
    uint8_t implem_address[ADDRESS_LENGTH];
} s_proxy_info;

typedef struct {
    uint8_t version;
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

#endif  // PROXY_INFO_H_

#endif
