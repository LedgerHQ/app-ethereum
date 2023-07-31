#include "shared_context.h"
#include "apdu_constants.h"

#include "feature_getPublicKey.h"
#include "ethUtils.h"
#include "common_ui.h"
#include "os_io_seproxyhal.h"

void handleGetPublicKey(uint8_t p1,
                        uint8_t p2,
                        const uint8_t *dataBuffer,
                        uint8_t dataLength,
                        unsigned int *flags,
                        unsigned int *tx) {
    uint8_t privateKeyData[INT256_LENGTH];
    bip32_path_t bip32;
    cx_ecfp_private_key_t privateKey;

    if (!G_called_from_swap) {
        reset_app_context();
    }

    if ((p1 != P1_CONFIRM) && (p1 != P1_NON_CONFIRM)) {
        THROW(0x6B00);
    }
    if ((p2 != P2_CHAINCODE) && (p2 != P2_NO_CHAINCODE)) {
        THROW(0x6B00);
    }

    dataBuffer = parseBip32(dataBuffer, &dataLength, &bip32);

    if (dataBuffer == NULL) {
        THROW(0x6a80);
    }

    tmpCtx.publicKeyContext.getChaincode = (p2 == P2_CHAINCODE);
    io_seproxyhal_io_heartbeat();
    os_perso_derive_node_bip32(
        CX_CURVE_256K1,
        bip32.path,
        bip32.length,
        privateKeyData,
        (tmpCtx.publicKeyContext.getChaincode ? tmpCtx.publicKeyContext.chainCode : NULL));
    cx_ecfp_init_private_key(CX_CURVE_256K1, privateKeyData, 32, &privateKey);
    io_seproxyhal_io_heartbeat();
    cx_ecfp_generate_pair(CX_CURVE_256K1, &tmpCtx.publicKeyContext.publicKey, &privateKey, 1);
    explicit_bzero(&privateKey, sizeof(privateKey));
    explicit_bzero(privateKeyData, sizeof(privateKeyData));
    io_seproxyhal_io_heartbeat();
    getEthAddressStringFromKey(&tmpCtx.publicKeyContext.publicKey,
                               tmpCtx.publicKeyContext.address,
                               &global_sha3,
                               chainConfig->chainId);
#ifndef NO_CONSENT
    if (p1 == P1_NON_CONFIRM)
#endif  // NO_CONSENT
    {
        *tx = set_result_get_publicKey();
        THROW(0x9000);
    }
#ifndef NO_CONSENT
    else {
        snprintf(strings.common.fullAddress,
                 sizeof(strings.common.fullAddress),
                 "0x%.*s",
                 40,
                 tmpCtx.publicKeyContext.address);
        ui_display_public_key();

        *flags |= IO_ASYNCH_REPLY;
    }
#endif  // NO_CONSENT
}
