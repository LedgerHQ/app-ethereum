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

void starkDerivePrivateKey(uint32_t *bip32Path, uint32_t bip32PathLength, uint8_t *privateKeyData);

void stark_get_amount_string(uint8_t *contractAddress,
                             uint8_t *quantum256,
                             uint8_t *amount64,
                             char *tmp100,
                             char *target100);

int stark_sign(uint8_t *signature, /* out */
               uint8_t *privateKeyData,
               FieldElement token1,
               FieldElement token2,
               FieldElement msg,
               FieldElement condition);

#endif  // _STARK_UTILS_H_
