#include "shared_context.h"
#include "apdu_constants.h"

#include "feature_performPrivacyOperation.h"
#include "common_ui.h"
#include "uint_common.h"

#define P2_PUBLIC_ENCRYPTION_KEY 0x00
#define P2_SHARED_SECRET         0x01

void decodeScalar(const uint8_t *scalarIn, uint8_t *scalarOut) {
    for (uint8_t i = 0; i < 32; i++) {
        switch (i) {
            case 0:
                scalarOut[0] = (scalarIn[31] & 0x7f) | 0x40;
                break;
            case 31:
                scalarOut[31] = scalarIn[0] & 0xf8;
                break;
            default:
                scalarOut[i] = scalarIn[31 - i];
        }
    }
}

uint16_t handlePerformPrivacyOperation(uint8_t p1,
                                       uint8_t p2,
                                       const uint8_t *dataBuffer,
                                       uint8_t dataLength,
                                       unsigned int *flags,
                                       unsigned int *tx) {
    cx_ecfp_private_key_t privateKey;
    uint8_t privateKeyData[64];
    uint8_t privateKeyDataSwapped[INT256_LENGTH];
    bip32_path_t bip32;
    cx_err_t error = CX_INTERNAL_ERROR;

    if ((p1 != P1_CONFIRM) && (p1 != P1_NON_CONFIRM)) {
        return APDU_RESPONSE_INVALID_P1_P2;
    }

    if ((p2 != P2_PUBLIC_ENCRYPTION_KEY) && (p2 != P2_SHARED_SECRET)) {
        return APDU_RESPONSE_INVALID_P1_P2;
    }

    dataBuffer = parseBip32(dataBuffer, &dataLength, &bip32);
    if (dataBuffer == NULL) {
        return APDU_RESPONSE_INVALID_DATA;
    }

    if ((p2 == P2_SHARED_SECRET) && (dataLength < 32)) {
        return APDU_RESPONSE_WRONG_DATA_LENGTH;
    }

    CX_CHECK(os_derive_bip32_no_throw(
        CX_CURVE_256K1,
        bip32.path,
        bip32.length,
        privateKeyData,
        (tmpCtx.publicKeyContext.getChaincode ? tmpCtx.publicKeyContext.chainCode : NULL)));
    CX_CHECK(cx_ecfp_init_private_key_no_throw(CX_CURVE_256K1, privateKeyData, 32, &privateKey));
    CX_CHECK(cx_ecfp_generate_pair_no_throw(CX_CURVE_256K1,
                                            &tmpCtx.publicKeyContext.publicKey,
                                            &privateKey,
                                            1));
    getEthAddressStringFromRawKey((const uint8_t *) &tmpCtx.publicKeyContext.publicKey.W,
                                  tmpCtx.publicKeyContext.address,
                                  chainConfig->chainId);
    if (p2 == P2_PUBLIC_ENCRYPTION_KEY) {
        decodeScalar(privateKeyData, privateKeyDataSwapped);
        CX_CHECK(cx_ecfp_init_private_key_no_throw(CX_CURVE_Curve25519,
                                                   privateKeyDataSwapped,
                                                   32,
                                                   &privateKey));
        CX_CHECK(cx_ecfp_generate_pair_no_throw(CX_CURVE_Curve25519,
                                                &tmpCtx.publicKeyContext.publicKey,
                                                &privateKey,
                                                1));
    } else {
        memmove(tmpCtx.publicKeyContext.publicKey.W + 1, dataBuffer, 32);
        CX_CHECK(cx_x25519(tmpCtx.publicKeyContext.publicKey.W + 1, privateKeyData, 32));
    }

    if (p1 == P1_NON_CONFIRM) {
        *tx = set_result_perform_privacy_operation();
        return APDU_RESPONSE_OK;
    }
    snprintf(strings.common.toAddress,
             sizeof(strings.common.toAddress),
             "0x%.*s",
             40,
             tmpCtx.publicKeyContext.address);
    for (uint8_t i = 0; i < 32; i++) {
        privateKeyData[i] = tmpCtx.publicKeyContext.publicKey.W[32 - i];
    }
    format_hex(privateKeyData,
               32,
               strings.common.fullAmount,
               sizeof(strings.common.fullAmount) - 1);

    if (p2 == P2_PUBLIC_ENCRYPTION_KEY) {
        ui_display_privacy_public_key();
    } else {
        ui_display_privacy_shared_secret();
    }
    *flags |= IO_ASYNCH_REPLY;
    // Return code will be sent after UI approve/cancel
    error = 0;
end:
    explicit_bzero(privateKeyDataSwapped, sizeof(privateKeyDataSwapped));
    explicit_bzero(&privateKey, sizeof(privateKey));
    explicit_bzero(privateKeyData, sizeof(privateKeyData));
    return error;
}
