#pragma once

#include "shared_context.h"

uint32_t set_result_get_eth2_publicKey(void);
uint32_t get_eth2_public_key(uint32_t *bip32Path, uint8_t bip32PathLength, uint8_t *out);
