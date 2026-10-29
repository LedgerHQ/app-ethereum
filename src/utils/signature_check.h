#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "os_pki.h"

bool check_signature_with_pubkey(uint8_t *hash,
                                 const uint8_t hash_len,
                                 const uint8_t keyUsageExp,
                                 const uint8_t *sig,
                                 const uint8_t sig_len);
