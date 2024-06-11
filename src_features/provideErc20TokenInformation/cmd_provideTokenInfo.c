#include "shared_context.h"
#include "apdu_constants.h"
#include "public_keys.h"
#include "common_ui.h"
#include "os_io_seproxyhal.h"
#include "network.h"
#include "manage_asset_info.h"

void handleProvideErc20TokenInformation(uint8_t p1,
                                        uint8_t p2,
                                        const uint8_t *workBuffer,
                                        uint8_t dataLength,
                                        unsigned int *flags,
                                        unsigned int *tx) {
    UNUSED(p1);
    UNUSED(p2);
    UNUSED(flags);
    UNUSED(tx);
    uint32_t offset = 0;
    uint8_t tickerLength;
    uint64_t chain_id;
    uint8_t hash[INT256_LENGTH];
    cx_ecfp_public_key_t tokenKey;

    tokenDefinition_t *token = &get_current_asset_info()->token;

    PRINTF("Provisioning currentAssetIndex %d\n", tmpCtx.transactionContext.currentAssetIndex);

    if (dataLength < 1) {
        THROW(0x6A80);
    }
    tickerLength = workBuffer[offset++];
    dataLength--;
    if ((tickerLength + 1) > sizeof(token->ticker)) {
        THROW(0x6A80);
    }
    if (dataLength < tickerLength + 20 + 4 + 4) {
        THROW(0x6A80);
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
        THROW(0x6A80);
    }
    offset += 4;
    dataLength -= 4;

    CX_ASSERT(cx_ecfp_init_public_key_no_throw(CX_CURVE_256K1,
                                               LEDGER_SIGNATURE_PUBLIC_KEY,
                                               sizeof(LEDGER_SIGNATURE_PUBLIC_KEY),
                                               &tokenKey));
    if (!cx_ecdsa_verify_no_throw(&tokenKey, hash, 32, workBuffer + offset, dataLength)) {
#ifndef HAVE_BYPASS_SIGNATURES
        PRINTF("Invalid token signature\n");
        THROW(0x6A80);
#endif
    }

    G_io_apdu_buffer[0] = tmpCtx.transactionContext.currentAssetIndex;
    validate_current_asset_info();
    U2BE_ENCODE(G_io_apdu_buffer, 1, APDU_RESPONSE_OK);
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 3);
}
