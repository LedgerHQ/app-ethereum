#pragma once

#include "shared_context.h"

uint16_t get_public_key(uint8_t *out, uint8_t outLength);
bool get_address_of_path(const bip32_path_t *bip32, uint8_t out[ADDRESS_LENGTH]);
uint16_t get_public_key_string(bip32_path_t *bip32,
                               uint8_t *pubKey,
                               char *address,
                               uint8_t *chainCode,
                               uint64_t chainId);
bool get_displayable_address(const bip32_path_t *bip32,
                             char *out,
                             size_t out_size,
                             uint64_t chain_id);
uint32_t set_result_get_publicKey(void);
