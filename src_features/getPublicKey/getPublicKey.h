#pragma once

#include "shared_context.h"

uint16_t get_public_key(uint8_t *out, uint8_t outLength);
uint16_t get_public_key_string(bip32_path_t *bip32,
                               uint8_t *pubKey,
                               char *address,
                               uint8_t *chainCode,
                               uint64_t chainId);
uint32_t set_result_get_publicKey(void);
