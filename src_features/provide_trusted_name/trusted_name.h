#pragma once

#ifdef HAVE_TRUSTED_NAME

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

const char *get_trusted_name(uint8_t type_count,
                             const e_name_type *types,
                             uint8_t source_count,
                             const e_name_source *sources,
                             const uint64_t *chain_id,
                             const uint8_t *addr);

extern char g_trusted_name[TRUSTED_NAME_MAX_LENGTH + 1];

bool handle_tlv_trusted_name_payload(const uint8_t *payload, uint16_t size, bool to_free);

#endif  // !HAVE_TRUSTED_NAME
