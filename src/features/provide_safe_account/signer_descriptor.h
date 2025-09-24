#pragma once

#ifdef HAVE_SAFE_ACCOUNT

#include <stdint.h>
#include "common_utils.h"

typedef struct {
    char address[ADDRESS_LENGTH];
} signer_data_t;

typedef struct {
    signer_data_t *data;
    bool is_valid;
} signers_descriptor_t;

extern signers_descriptor_t SIGNER_DESC;

bool handle_signer_tlv_payload(const uint8_t *payload, uint16_t size);
void clear_signer_descriptor(void);

#endif  // HAVE_SAFE_ACCOUNT
