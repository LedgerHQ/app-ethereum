#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "cx.h"
#include "tlv_library.h"
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
    s_proxy_info proxy_info;
    uint8_t sig_size;
    const uint8_t *sig;
    cx_sha256_t struct_hash;
    TLV_reception_t received_tags;
} s_proxy_info_ctx;

bool handle_proxy_info_tlv_payload(const buffer_t *buf, s_proxy_info_ctx *context);
bool verify_proxy_info_struct(const s_proxy_info_ctx *context);
const uint8_t *get_proxy_contract(const uint64_t *chain_id,
                                  const uint8_t *addr,
                                  const uint8_t *selector);
const uint8_t *get_implem_contract(const uint64_t *chain_id,
                                   const uint8_t *addr,
                                   const uint8_t *selector);
void proxy_cleanup(void);
