#include <string.h>
#include "crypto_helpers.h"
#include "shared_context.h"
#include "apdu_constants.h"

uint16_t get_public_key_string(bip32_path_t *bip32,
                               uint8_t *pubKey,
                               char *address,
                               uint8_t *chainCode,
                               uint64_t chainId) {
    cx_err_t error = CX_INTERNAL_ERROR;

    CX_CHECK(bip32_derive_get_pubkey_256(CX_CURVE_256K1,
                                         bip32->path,
                                         bip32->length,
                                         pubKey,
                                         chainCode,
                                         CX_SHA512));
    getEthAddressStringFromRawKey(pubKey, address, chainId);
    error = CX_OK;
end:
    return error;
}

uint16_t get_public_key(uint8_t *out, uint8_t outLength) {
    uint8_t raw_pubkey[65];
    cx_err_t error = CX_INTERNAL_ERROR;

    if (outLength < ADDRESS_LENGTH) {
        return APDU_RESPONSE_WRONG_DATA_LENGTH;
    }
    CX_CHECK(bip32_derive_get_pubkey_256(CX_CURVE_256K1,
                                         tmpCtx.transactionContext.bip32.path,
                                         tmpCtx.transactionContext.bip32.length,
                                         raw_pubkey,
                                         NULL,
                                         CX_SHA512));

    getEthAddressFromRawKey(raw_pubkey, out);
    error = APDU_RESPONSE_OK;
end:
    return error;
}

uint32_t set_result_get_publicKey() {
    uint32_t tx = 0;
    G_io_apdu_buffer[tx++] = 65;
    memmove(G_io_apdu_buffer + tx, tmpCtx.publicKeyContext.publicKey.W, 65);
    tx += 65;
    G_io_apdu_buffer[tx++] = 40;
    memmove(G_io_apdu_buffer + tx, tmpCtx.publicKeyContext.address, 40);
    tx += 40;
    if (tmpCtx.publicKeyContext.getChaincode) {
        memmove(G_io_apdu_buffer + tx, tmpCtx.publicKeyContext.chainCode, 32);
        tx += 32;
    }
    return tx;
}
