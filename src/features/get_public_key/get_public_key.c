#include <string.h>
#include "crypto_helpers.h"
#include "shared_context.h"
#include "apdu_constants.h"
#include "ox_ec.h"

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

/**
 * Derive the address of a BIP32 path
 *
 * @param[in] bip32 derivation path
 * @param[out] out the address, ADDRESS_LENGTH bytes
 * @return whether it was successful
 */
bool get_address_of_path(const bip32_path_t *bip32, uint8_t out[ADDRESS_LENGTH]) {
    uint8_t raw_pubkey[CX_SECP256_PUB_KEY_SIZE];

    if (bip32_derive_get_pubkey_256(CX_CURVE_256K1,
                                    bip32->path,
                                    bip32->length,
                                    raw_pubkey,
                                    NULL,
                                    CX_SHA512) != CX_OK) {
        return false;
    }
    getEthAddressFromRawKey(raw_pubkey, out);
    return true;
}

/**
 * Derive the address of a BIP32 path, formatted as "0x" followed by its checksummed hex
 *
 * @param[in] bip32 derivation path
 * @param[out] out output buffer, at least ADDRESS_LENGTH_HEX_STR bytes
 * @param[in] out_size size of out
 * @param[in] chain_id chain ID, selecting the checksum scheme
 * @return whether it was successful
 */
bool get_displayable_address(const bip32_path_t *bip32,
                             char *out,
                             size_t out_size,
                             uint64_t chain_id) {
    uint8_t address[ADDRESS_LENGTH];

    if (!get_address_of_path(bip32, address)) {
        return false;
    }
    return getEthDisplayableAddress(address, out, out_size, chain_id);
}

uint16_t get_public_key(uint8_t *out, uint8_t outLength) {
    uint8_t raw_pubkey[CX_SECP256_PUB_KEY_SIZE];
    cx_err_t error;

    if (outLength < ADDRESS_LENGTH) {
        return SWO_WRONG_DATA_LENGTH;
    }
    if ((error = bip32_derive_get_pubkey_256(CX_CURVE_256K1,
                                             tmpCtx.transactionContext.bip32.path,
                                             tmpCtx.transactionContext.bip32.length,
                                             raw_pubkey,
                                             NULL,
                                             CX_SHA512)) != CX_OK) {
        PRINTF("Error: could not derive pubkey!\n");
        return error;
    }
    getEthAddressFromRawKey(raw_pubkey, out);
    return SWO_SUCCESS;
}

uint32_t set_result_get_publicKey() {
    uint32_t tx = 0;
    G_io_tx_buffer[tx++] = CX_SECP256_PUB_KEY_SIZE;
    memmove(G_io_tx_buffer + tx, tmpCtx.publicKeyContext.publicKey.W, CX_SECP256_PUB_KEY_SIZE);
    tx += CX_SECP256_PUB_KEY_SIZE;
    G_io_tx_buffer[tx++] = ADDRESS_LENGTH_HEX;
    memmove(G_io_tx_buffer + tx, tmpCtx.publicKeyContext.address, ADDRESS_LENGTH_HEX);
    tx += ADDRESS_LENGTH_HEX;
    if (tmpCtx.publicKeyContext.getChaincode) {
        memmove(G_io_tx_buffer + tx, tmpCtx.publicKeyContext.chainCode, INT256_LENGTH);
        tx += INT256_LENGTH;
    }
    return tx;
}
