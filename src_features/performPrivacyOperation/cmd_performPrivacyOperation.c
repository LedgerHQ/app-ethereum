#include "shared_context.h"
#include "apdu_constants.h"
#include "ethUtils.h"

#include "ui_flow.h"
#include "feature_performPrivacyOperation.h"

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

void handlePerformPrivacyOperation(uint8_t p1,
                                   uint8_t p2,
                                   uint8_t *dataBuffer,
                                   uint16_t dataLength,
                                   unsigned int *flags,
                                   unsigned int *tx) {
    UNUSED(dataLength);
    uint8_t privateKeyData[INT256_LENGTH];
    uint8_t privateKeyDataSwapped[INT256_LENGTH];
    uint32_t bip32Path[MAX_BIP32_PATH];
    uint8_t bip32PathLength = *(dataBuffer++);
    cx_err_t status = CX_OK;
    if (p2 == P2_PUBLIC_ENCRYPTION_KEY) {
        if (dataLength < 1 + 4 * bip32PathLength) {
            THROW(0x6700);
        }
    } else if (p2 == P2_SHARED_SECRET) {
        if (dataLength < 1 + 4 * bip32PathLength + 32) {
            THROW(0x6700);
        }
    } else {
        THROW(0x6B00);
    }
    cx_ecfp_private_key_t privateKey;
    if ((bip32PathLength < 0x01) || (bip32PathLength > MAX_BIP32_PATH)) {
        PRINTF("Invalid path\n");
        THROW(0x6a80);
    }
    if ((p1 != P1_CONFIRM) && (p1 != P1_NON_CONFIRM)) {
        THROW(0x6B00);
    }
    for (uint8_t i = 0; i < bip32PathLength; i++) {
        bip32Path[i] = U4BE(dataBuffer, 0);
        dataBuffer += 4;
    }
    os_perso_derive_node_bip32(
        CX_CURVE_256K1,
        bip32Path,
        bip32PathLength,
        privateKeyData,
        (tmpCtx.publicKeyContext.getChaincode ? tmpCtx.publicKeyContext.chainCode : NULL));
    cx_ecfp_init_private_key(CX_CURVE_256K1, privateKeyData, 32, &privateKey);
    cx_ecfp_generate_pair(CX_CURVE_256K1, &tmpCtx.publicKeyContext.publicKey, &privateKey, 1);
    getEthAddressStringFromKey(&tmpCtx.publicKeyContext.publicKey,
                               tmpCtx.publicKeyContext.address,
                               &global_sha3,
                               chainConfig->chainId);
    if (p2 == P2_PUBLIC_ENCRYPTION_KEY) {
        decodeScalar(privateKeyData, privateKeyDataSwapped);
        cx_ecfp_init_private_key(CX_CURVE_Curve25519, privateKeyDataSwapped, 32, &privateKey);
        cx_ecfp_generate_pair(CX_CURVE_Curve25519,
                              &tmpCtx.publicKeyContext.publicKey,
                              &privateKey,
                              1);
        explicit_bzero(privateKeyDataSwapped, sizeof(privateKeyDataSwapped));
    } else {
        memmove(tmpCtx.publicKeyContext.publicKey.W + 1, dataBuffer, 32);
        status = cx_x25519(tmpCtx.publicKeyContext.publicKey.W + 1, privateKeyData, 32);
    }
    explicit_bzero(&privateKey, sizeof(privateKey));
    explicit_bzero(privateKeyData, sizeof(privateKeyData));

    if (status != CX_OK) {
        THROW(0x6A80);
    }

#ifndef NO_CONSENT
    if (p1 == P1_NON_CONFIRM)
#endif  // NO_CONSENT
    {
        *tx = set_result_perform_privacy_operation();
        THROW(0x9000);
    }
#ifndef NO_CONSENT
    else {
        snprintf(strings.common.fullAddress,
                 sizeof(strings.common.fullAddress),
                 "0x%.*s",
                 40,
                 tmpCtx.publicKeyContext.address);
        for (uint8_t i = 0; i < 32; i++) {
            privateKeyData[i] = tmpCtx.publicKeyContext.publicKey.W[32 - i];
        }
        snprintf(strings.common.fullAmount,
                 sizeof(strings.common.fullAmount) - 1,
                 "%.*H",
                 32,
                 privateKeyData);
        if (p2 == P2_PUBLIC_ENCRYPTION_KEY) {
            ux_flow_init(0, ux_display_privacy_public_key_flow, NULL);
        } else {
            ux_flow_init(0, ux_display_privacy_shared_secret_flow, NULL);
        }

        *flags |= IO_ASYNCH_REPLY;
    }
#endif  // NO_CONSENT
}
