#ifdef HAVE_ETH2

#include "shared_context.h"
#include "apdu_constants.h"

#include "ui_flow.h"
#include "feature_getEth2PublicKey.h"

static const uint8_t BLS12_381_FIELD_MODULUS[] = {
    0x1a, 0x01, 0x11, 0xea, 0x39, 0x7f, 0xe6, 0x9a, 0x4b, 0x1b, 0xa7, 0xb6, 0x43, 0x4b, 0xac, 0xd7,
    0x64, 0x77, 0x4b, 0x84, 0xf3, 0x85, 0x12, 0xbf, 0x67, 0x30, 0xd2, 0xa0, 0xf6, 0xb0, 0xf6, 0x24,
    0x1e, 0xab, 0xff, 0xfe, 0xb1, 0x53, 0xff, 0xff, 0xb9, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xaa, 0xab};

void getEth2PublicKey(uint32_t *bip32Path, uint8_t bip32PathLength, uint8_t *out) {
    uint8_t privateKeyData[INT256_LENGTH];
    cx_ecfp_256_extended_private_key_t privateKey;
    cx_ecfp_384_public_key_t publicKey;
    uint8_t yFlag = 0;
    uint8_t tmp[96];

    io_seproxyhal_io_heartbeat();
    os_perso_derive_eip2333(CX_CURVE_BLS12_381_G1, bip32Path, bip32PathLength, privateKeyData);
    io_seproxyhal_io_heartbeat();
    memset(tmp, 0, 48);
    memmove(tmp + 16, privateKeyData, 32);
    cx_ecfp_init_private_key(CX_CURVE_BLS12_381_G1, tmp, 48, (cx_ecfp_private_key_t *) &privateKey);
    cx_ecfp_generate_pair(CX_CURVE_BLS12_381_G1,
                          (cx_ecfp_public_key_t *) &publicKey,
                          (cx_ecfp_private_key_t *) &privateKey,
                          1);
    explicit_bzero(tmp, 96);
    explicit_bzero((void *) &privateKey, sizeof(cx_ecfp_256_extended_private_key_t));
    tmp[47] = 2;
    cx_math_mult(tmp, publicKey.W + 1 + 48, tmp, 48);
    if (cx_math_cmp(tmp + 48, BLS12_381_FIELD_MODULUS, 48) > 0) {
        yFlag = 0x20;
    }
    publicKey.W[1] &= 0x1f;
    publicKey.W[1] |= 0x80 | yFlag;
    memmove(out, publicKey.W + 1, 48);
}

void handleGetEth2PublicKey(uint8_t p1,
                            uint8_t p2,
                            uint8_t *dataBuffer,
                            uint16_t dataLength,
                            unsigned int *flags,
                            unsigned int *tx) {
    UNUSED(dataLength);
    uint32_t bip32Path[MAX_BIP32_PATH];
    uint32_t i;
    uint8_t bip32PathLength = *(dataBuffer++);

    if (!called_from_swap) {
        reset_app_context();
    }
    if ((bip32PathLength < 0x01) || (bip32PathLength > MAX_BIP32_PATH)) {
        PRINTF("Invalid path\n");
        THROW(0x6a80);
    }
    if ((p1 != P1_CONFIRM) && (p1 != P1_NON_CONFIRM)) {
        THROW(0x6B00);
    }
    if (p2 != 0) {
        THROW(0x6B00);
    }
    for (i = 0; i < bip32PathLength; i++) {
        bip32Path[i] = U4BE(dataBuffer, 0);
        dataBuffer += 4;
    }
    getEth2PublicKey(bip32Path, bip32PathLength, tmpCtx.publicKeyContext.publicKey.W);

#ifndef NO_CONSENT
    if (p1 == P1_NON_CONFIRM)
#endif  // NO_CONSENT
    {
        *tx = set_result_get_eth2_publicKey();
        THROW(0x9000);
    }
#ifndef NO_CONSENT
    else {
        ux_flow_init(0, ux_display_public_eth2_flow, NULL);

        *flags |= IO_ASYNCH_REPLY;
    }
#endif  // NO_CONSENT
}

#endif
