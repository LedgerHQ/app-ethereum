#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "common_utils.h"  // ADDRESS_LENGTH
#include "tlv.h"
#include "signature.h"

#define TRUSTED_NAME_MAX_LENGTH 30

typedef enum {
    TN_TYPE_ACCOUNT = 1,
    TN_TYPE_CONTRACT,
    TN_TYPE_NFT_COLLECTION,
    TN_TYPE_TOKEN,
    TN_TYPE_WALLET,
    TN_TYPE_CONTEXT_ADDRESS,
    _TN_TYPE_COUNT_,
} e_name_type;

// because the enum does not start at 0
#define TN_TYPE_COUNT (_TN_TYPE_COUNT_ - TN_TYPE_ACCOUNT)

typedef enum {
    TN_SOURCE_LAB = 0,
    TN_SOURCE_CAL,
    TN_SOURCE_ENS,
    TN_SOURCE_UD,
    TN_SOURCE_FN,
    TN_SOURCE_DNS,
    TN_SOURCE_DYNAMIC_RESOLVER,
    TN_SOURCE_COUNT,
} e_name_source;

typedef enum { TN_KEY_ID_DOMAIN_SVC = 0x07, TN_KEY_ID_CAL = 0x09 } e_tn_key_id;

typedef struct {
    bool valid;
    uint8_t struct_version;
    char *name;
    uint8_t addr[ADDRESS_LENGTH];
    uint64_t chain_id;
    e_name_type name_type;
    e_name_source name_source;
    uint8_t nft_id[INT256_LENGTH];
} s_trusted_name_info;

typedef struct {
    s_trusted_name_info trusted_name;
    e_tn_key_id key_id;
    uint8_t input_sig_size;
    uint8_t input_sig[ECDSA_SIGNATURE_MAX_LENGTH];
    cx_sha256_t hash_ctx;
    uint32_t rcv_flags;
} s_trusted_name_ctx;

const char *get_trusted_name(uint8_t type_count,
                             const e_name_type *types,
                             uint8_t source_count,
                             const e_name_source *sources,
                             const uint64_t *chain_id,
                             const uint8_t *addr);

extern s_trusted_name_info *g_trusted_name_info;
extern char *g_trusted_name;

bool handle_trusted_name_struct(const s_tlv_data *data, s_trusted_name_ctx *context);
bool verify_trusted_name_struct(const s_trusted_name_ctx *ctx);
