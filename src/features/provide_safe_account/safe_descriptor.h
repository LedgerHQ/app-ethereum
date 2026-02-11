#pragma once

#include <stdint.h>
#include "common_utils.h"
#include "buffer.h"

// clang-format off
typedef enum {
    ROLE_SIGNER,
    ROLE_PROPOSER,
    ROLE_MAX
} safe_role_t;

#define ROLE_STR(x)                   \
    (x == ROLE_SIGNER   ? "Signer"  : \
     x == ROLE_PROPOSER ? "Proposer": \
                          "Unknown")

typedef struct {
    const char address[ADDRESS_LENGTH];
    uint16_t threshold;
    uint16_t signers_count;
    safe_role_t role;
} safe_descriptor_t;
// clang-format on

extern safe_descriptor_t *SAFE_DESC;

bool handle_safe_tlv_payload(const buffer_t *payload);
void clear_safe_descriptor(void);
