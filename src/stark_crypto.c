#ifdef HAVE_STARKWARE

#include "shared_context.h"
#include "stark_utils.h"
#include "utils.h"
#include "ethUtils.h"

extraInfo_t *getKnownToken(uint8_t *contractAddress);

static unsigned char const C_cx_Stark256_n[] = {
    // n: 0x0800000000000010ffffffffffffffffb781126dcae7b2321e66a241adc64d2f
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xb7, 0x81, 0x12, 0x6d, 0xca, 0xe7, 0xb2, 0x32, 0x1e, 0x66, 0xa2, 0x41, 0xad, 0xc6, 0x4d, 0x2f};

// C_cx_secp256k1_n - (C_cx_secp256k1_n % C_cx_Stark256_n)
static unsigned char const STARK_DERIVE_BIAS[] = {
    0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf7,
    0x38, 0xa1, 0x3b, 0x4b, 0x92, 0x0e, 0x94, 0x11, 0xae, 0x6d, 0xa5, 0xf4, 0x0b, 0x03, 0x58, 0xb1};

WARN_UNUSED_RESULT cx_err_t stark_init_privkey(const uint32_t *bip32Path,
                                               uint32_t bip32PathLength,
                                               cx_ecfp_256_private_key_t *privkey) {
    cx_err_t error = CX_OK;
    uint8_t stark_privkey[32];
    uint8_t raw_privkey[64];
    uint8_t index = 0;

    // Sanity check
    if ((bip32PathLength < 2) || (bip32Path[0] != STARK_BIP32_PATH_0) ||
        (bip32Path[1] != STARK_BIP32_PATH_1)) {
        PRINTF("Invalid Stark derivation path %d %d\n", bip32Path[0], bip32Path[1]);
        THROW(0x6a80);
    }

    // Derive private key according to BIP32 path
    CX_CHECK(
        os_derive_bip32_no_throw(CX_CURVE_256K1, bip32Path, bip32PathLength, raw_privkey, NULL));

    PRINTF("Private key before processing %.*H\n", 32, raw_privkey);

    for (;;) {
        raw_privkey[32] = index;
        cx_hash_sha256(raw_privkey, 33, stark_privkey, 32);
        PRINTF("Key hash %.*H\n", 32, stark_privkey);
        if (cx_math_cmp(stark_privkey, STARK_DERIVE_BIAS, 32) < 0) {
            cx_math_modm(stark_privkey, 32, C_cx_Stark256_n, 32);
            break;
        }
        index++;
    }
    PRINTF("Key result %.*H\n", 32, stark_privkey);

    // Init privkey from raw
    CX_CHECK(cx_ecfp_init_private_key_no_throw(CX_CURVE_Stark256, stark_privkey, 32, privkey));

end:
    explicit_bzero(raw_privkey, sizeof(raw_privkey));
    explicit_bzero(stark_privkey, sizeof(stark_privkey));

    return error;
}

WARN_UNUSED_RESULT cx_err_t stark_get_pubkey(const uint32_t *bip32Path,
                                             uint32_t bip32PathLength,
                                             uint8_t raw_pubkey[static 65]) {
    cx_err_t error = CX_OK;
    cx_ecfp_256_private_key_t privkey;
    cx_ecfp_256_public_key_t pubkey;

    // Derive private key according to BIP32 path
    CX_CHECK(stark_init_privkey(bip32Path, bip32PathLength, &privkey));

    // Generate associated pubkey
    CX_CHECK(cx_ecfp_generate_pair_no_throw(CX_CURVE_Stark256, &pubkey, &privkey, true));

    // Check pubkey length then copy it to raw_pubkey
    if (pubkey.W_len != 65) {
        error = CX_EC_INVALID_CURVE;
        goto end;
    }
    memmove(raw_pubkey, pubkey.W, pubkey.W_len);

end:
    explicit_bzero(&privkey, sizeof(privkey));

    return error;
}

WARN_UNUSED_RESULT cx_err_t stark_sign_hash(const uint32_t *bip32Path,
                                            uint32_t bip32PathLength,
                                            const uint8_t *hash,
                                            uint32_t hash_len,
                                            uint8_t sig[static 65]) {
    cx_err_t error = CX_OK;
    cx_ecfp_256_private_key_t privkey;
    unsigned int info = 0;

    // Derive private key according to BIP32 path
    CX_CHECK(stark_init_privkey(bip32Path, bip32PathLength, &privkey));

    sig[0] = 0;
    CX_CHECK(cx_ecdsa_sign_rs_no_throw(&privkey,
                                       CX_RND_RFC6979 | CX_LAST,
                                       CX_SHA256,
                                       hash,
                                       hash_len,
                                       32,
                                       sig + 1,
                                       sig + 1 + 32,
                                       &info));

end:
    explicit_bzero(&privkey, sizeof(privkey));

    return error;
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
        tokenDefinition_t *token = &getKnownToken(contractAddress)->token;
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
