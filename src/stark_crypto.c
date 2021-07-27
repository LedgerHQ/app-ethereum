#ifdef HAVE_STARKWARE

#include "shared_context.h"
#include "stark_utils.h"
#include "ui_callbacks.h"
#include "utils.h"

static unsigned char const C_cx_Stark256_n[] = {
    // n: 0x0800000000000010ffffffffffffffffb781126dcae7b2321e66a241adc64d2f
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xb7, 0x81, 0x12, 0x6d, 0xca, 0xe7, 0xb2, 0x32, 0x1e, 0x66, 0xa2, 0x41, 0xad, 0xc6, 0x4d, 0x2f};

// C_cx_secp256k1_n - (C_cx_secp256k1_n % C_cx_Stark256_n)
static unsigned char const STARK_DERIVE_BIAS[] = {
    0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf7,
    0x38, 0xa1, 0x3b, 0x4b, 0x92, 0x0e, 0x94, 0x11, 0xae, 0x6d, 0xa5, 0xf4, 0x0b, 0x03, 0x58, 0xb1};

void starkDerivePrivateKey(uint32_t *bip32Path, uint32_t bip32PathLength, uint8_t *privateKeyData) {
#if 0  
  // Sanity check 
  if (bip32Path[0] != STARK_BIP32_PATH_0)  {
    PRINTF("Invalid Stark derivation path %d\n", bip32Path[0]);
    THROW(0x6a80);
  }
  os_perso_derive_node_bip32(CX_CURVE_256K1, bip32Path, bip32PathLength, privateKeyData, NULL);  
  PRINTF("Private key before processing %.*H\n", 32, privateKeyData);
  // TODO - support additional schemes
  cx_math_modm(privateKeyData, 32, C_cx_Stark256_n, 32);
  PRINTF("Private key after processing %.*H\n", 32, privateKeyData);
#else
    uint8_t tmp[33];
    uint8_t index = 0;
    // Sanity check
    if ((bip32PathLength < 2) || (bip32Path[0] != STARK_BIP32_PATH_0) ||
        (bip32Path[1] != STARK_BIP32_PATH_1)) {
        PRINTF("Invalid Stark derivation path %d %d\n", bip32Path[0], bip32Path[1]);
        THROW(0x6a80);
    }
    os_perso_derive_node_bip32(CX_CURVE_256K1, bip32Path, bip32PathLength, tmp, NULL);
    PRINTF("Private key before processing %.*H\n", 32, tmp);
    for (;;) {
        tmp[32] = index;
        cx_hash_sha256(tmp, 33, privateKeyData, 32);
        PRINTF("Key hash %.*H\n", 32, privateKeyData);
        if (cx_math_cmp(privateKeyData, STARK_DERIVE_BIAS, 32) < 0) {
            cx_math_modm(privateKeyData, 32, C_cx_Stark256_n, 32);
            break;
        }
        index++;
    }
    PRINTF("Key result %.*H\n", 32, privateKeyData);

#endif
}

void stark_get_amount_string(uint8_t *contractAddress,
                             uint8_t *quantum256,
                             uint8_t *amount64,
                             char *tmp100,
                             char *target100) {
    uint256_t amountPre, quantum, amount;
    uint8_t decimals;
    char *ticker = chainConfig->coinName;

    PRINTF("stark_get_amount_string %.*H\n", 20, contractAddress);

    if (allzeroes(contractAddress, 20)) {
        decimals = WEI_TO_ETHER;
        PRINTF("stark_get_amount_string - ETH\n");
    } else {
        tokenDefinition_t *token = getKnownToken(contractAddress);
        if (token == NULL) {  // caught earlier
            THROW(0x6A80);
        }
        decimals = token->decimals;
        ticker = (char *) token->ticker;
        PRINTF("stark_get_amount_string - decimals %d ticker %s\n", decimals, ticker);
    }
    convertUint256BE(amount64, 8, &amountPre);
    readu256BE(quantum256, &quantum);
    mul256(&amountPre, &quantum, &amount);
    tostring256(&amount, 10, tmp100, 100);
    PRINTF("stark_get_amount_string - mul256 %s\n", tmp100);
    strlcpy(target100, ticker, 100);
    adjustDecimals(tmp100, strlen(tmp100), target100 + strlen(ticker), 100, decimals);
    PRINTF("get_amount_string %s\n", target100);
}

#endif  // HAVE_STARK
