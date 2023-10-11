#ifndef _STARK_UTILS_H_
#define _STARK_UTILS_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "os.h"
#include "cx.h"
#include "stark_crypto.h"

void compute_token_id(cx_sha3_t *sha3,
                      uint8_t *contractAddress,
                      uint8_t quantumType,
                      uint8_t *quantum,
                      uint8_t *mintingBlob,
                      bool assetTypeOnly,
                      uint8_t *output);

WARN_UNUSED_RESULT cx_err_t stark_get_pubkey(const uint32_t *bip32Path,
                                             uint32_t bip32PathLength,
                                             uint8_t raw_pubkey[static 65]);

WARN_UNUSED_RESULT cx_err_t stark_sign_hash(const uint32_t *bip32Path,
                                            uint32_t bip32PathLength,
                                            const uint8_t *hash,
                                            uint32_t hash_len,
                                            uint8_t sig[static 65]);

void stark_get_amount_string(uint8_t *contractAddress,
                             uint8_t *quantum256,
                             uint8_t *amount64,
                             char *tmp100,
                             char *target100);

void stark_hash(FieldElement hash, /* out */
                FieldElement token1,
                FieldElement token2,
                FieldElement msg,
                FieldElement condition);

#endif  // _STARK_UTILS_H_
