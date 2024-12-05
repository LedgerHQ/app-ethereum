#include "shared_context.h"
#include "apdu_constants.h"
#include "public_keys.h"
#include "common_ui.h"
#include "os_io_seproxyhal.h"
#include "network.h"
#include "manage_asset_info.h"
#ifdef HAVE_LEDGER_PKI
#include "os_pki.h"
#endif

uint16_t handleProvideErc20TokenInformation(const uint8_t *workBuffer,
                                            uint8_t dataLength,
                                            unsigned int *tx) {
    uint32_t offset = 0;
    uint8_t tickerLength;
    uint64_t chain_id;
    uint8_t hash[INT256_LENGTH];
    tokenDefinition_t *token = &get_current_asset_info()->token;
    cx_err_t error = CX_INTERNAL_ERROR;

    PRINTF("Provisioning currentAssetIndex %d\n", tmpCtx.transactionContext.currentAssetIndex);

    if (dataLength < 1) {
        return APDU_RESPONSE_INVALID_DATA;
    }
    tickerLength = workBuffer[offset++];
    dataLength--;
    if ((tickerLength + 1) > sizeof(token->ticker)) {
        return APDU_RESPONSE_INVALID_DATA;
    }
    if (dataLength < tickerLength + 20 + 4 + 4) {
        return APDU_RESPONSE_INVALID_DATA;
    }
    cx_hash_sha256(workBuffer + offset, tickerLength + 20 + 4 + 4, hash, 32);
    memmove(token->ticker, workBuffer + offset, tickerLength);
    token->ticker[tickerLength] = '\0';
    offset += tickerLength;
    dataLength -= tickerLength;
    memmove(token->address, workBuffer + offset, 20);
    offset += 20;
    dataLength -= 20;
    // TODO: 4 bytes for this is overkill
    token->decimals = U4BE(workBuffer, offset);
    offset += 4;
    dataLength -= 4;
    // TODO: Handle 64-bit long chain IDs
    chain_id = U4BE(workBuffer, offset);
    if (!app_compatible_with_chain_id(&chain_id)) {
        UNSUPPORTED_CHAIN_ID_MSG(chain_id);
        return APDU_RESPONSE_INVALID_DATA;
    }
    offset += 4;
    dataLength -= 4;

    error = check_signature_with_pubkey("ERC20 Token Info",
                                        hash,
                                        sizeof(hash),
                                        LEDGER_SIGNATURE_PUBLIC_KEY,
                                        sizeof(LEDGER_SIGNATURE_PUBLIC_KEY),
#ifdef HAVE_LEDGER_PKI
                                        CERTIFICATE_PUBLIC_KEY_USAGE_COIN_META,
#endif
                                        (uint8_t *) (workBuffer + offset),
                                        dataLength);
    if (error != CX_OK) {
        PRINTF("Invalid token signature\n");
#ifndef HAVE_BYPASS_SIGNATURES
        return APDU_RESPONSE_INVALID_DATA;
#endif
    }

    G_io_apdu_buffer[0] = tmpCtx.transactionContext.currentAssetIndex;
    validate_current_asset_info();
    *tx += 1;
    return APDU_RESPONSE_OK;
}
