#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "common_utils.h"
#include "buffer.h"

typedef struct {
    char address[ADDRESS_LENGTH];
} signer_data_t;

typedef struct {
    signer_data_t *data;
    bool is_valid;
} signers_descriptor_t;

extern signers_descriptor_t SIGNER_DESC;

bool handle_signer_tlv_payload(const buffer_t *payload);
void clear_signer_descriptor(void);
