#ifdef HAVE_TRUSTED_NAME

#ifndef TRUSTED_NAME_H_
#define TRUSTED_NAME_H_

#include <stdint.h>
#include <stdbool.h>

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
uint16_t handle_provide_trusted_name(uint8_t p1, const uint8_t *data, uint8_t length);

extern char g_trusted_name[TRUSTED_NAME_MAX_LENGTH + 1];

#endif  // TRUSTED_NAME_H_

#endif  // HAVE_TRUSTED_NAME
