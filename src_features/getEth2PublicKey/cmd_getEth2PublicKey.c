#ifdef HAVE_ETH2

#include "shared_context.h"
#include "apdu_constants.h"

#include "feature_getEth2PublicKey.h"
#include "common_ui.h"
#include "os_io_seproxyhal.h"

static const uint8_t BLS12_381_FIELD_MODULUS[] = {
    0x1a, 0x01, 0x11, 0xea, 0x39, 0x7f, 0xe6, 0x9a, 0x4b, 0x1b, 0xa7, 0xb6, 0x43, 0x4b, 0xac, 0xd7,
    0x64, 0x77, 0x4b, 0x84, 0xf3, 0x85, 0x12, 0xbf, 0x67, 0x30, 0xd2, 0xa0, 0xf6, 0xb0, 0xf6, 0x24,
    0x1e, 0xab, 0xff, 0xfe, 0xb1, 0x53, 0xff, 0xff, 0xb9, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xaa, 0xab};

void getEth2PublicKey(uint32_t *bip32Path, uint8_t bip32PathLength, uint8_t *out) {
    uint8_t privateKeyData[64];
    cx_ecfp_256_extended_private_key_t privateKey;
    cx_ecfp_384_public_key_t publicKey;
    uint8_t yFlag = 0;
    uint8_t tmp[96];
    int diff;

    io_seproxyhal_io_heartbeat();
    CX_ASSERT(os_derive_eip2333_no_throw(CX_CURVE_BLS12_381_G1,
                                         bip32Path,
                                         bip32PathLength,
                                         privateKeyData));
    io_seproxyhal_io_heartbeat();
    memset(tmp, 0, 48);
    memmove(tmp + 16, privateKeyData, 32);
    CX_ASSERT(cx_ecfp_init_private_key_no_throw(CX_CURVE_BLS12_381_G1,
                                                tmp,
                                                48,
                                                (cx_ecfp_private_key_t *) &privateKey));
    CX_ASSERT(cx_ecfp_generate_pair_no_throw(CX_CURVE_BLS12_381_G1,
                                             (cx_ecfp_public_key_t *) &publicKey,
                                             (cx_ecfp_private_key_t *) &privateKey,
                                             1));
    explicit_bzero(tmp, 96);
    explicit_bzero((void *) &privateKey, sizeof(cx_ecfp_256_extended_private_key_t));
    tmp[47] = 2;
    CX_ASSERT(cx_math_mult_no_throw(tmp, publicKey.W + 1 + 48, tmp, 48));
    CX_ASSERT(cx_math_cmp_no_throw(tmp + 48, BLS12_381_FIELD_MODULUS, 48, &diff));
    if (diff > 0) {
        yFlag = 0x20;
    }
    publicKey.W[1] &= 0x1f;
    publicKey.W[1] |= 0x80 | yFlag;
    memmove(out, publicKey.W + 1, 48);
}

void handleGetEth2PublicKey(uint8_t p1,
                            uint8_t p2,
                            const uint8_t *dataBuffer,
                            uint8_t dataLength,
                            unsigned int *flags,
                            unsigned int *tx) {
    bip32_path_t bip32;

    if (!G_called_from_swap) {
        reset_app_context();
    }
    if ((p1 != P1_CONFIRM) && (p1 != P1_NON_CONFIRM)) {
        THROW(APDU_RESPONSE_INVALID_P1_P2);
    }
    if (p2 != 0) {
        THROW(APDU_RESPONSE_INVALID_P1_P2);
    }

    dataBuffer = parseBip32(dataBuffer, &dataLength, &bip32);

    if (dataBuffer == NULL) {
        THROW(APDU_RESPONSE_INVALID_DATA);
    }

    getEth2PublicKey(bip32.path, bip32.length, tmpCtx.publicKeyContext.publicKey.W);

    if (p1 == P1_NON_CONFIRM) {
        *tx = set_result_get_eth2_publicKey();
        THROW(APDU_RESPONSE_OK);
    } else {
        ui_display_public_eth2();

        *flags |= IO_ASYNCH_REPLY;
    }
}

#endif
