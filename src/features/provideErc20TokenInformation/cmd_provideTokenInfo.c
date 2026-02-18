#include "apdu_constants.h"
#include "public_keys.h"
#include "network.h"
#include "manage_asset_info.h"
#include "os_pki.h"

uint16_t handleProvideErc20TokenInformation(const uint8_t *workBuffer,
                                            uint8_t dataLength,
                                            unsigned int *tx) {
    uint32_t offset = 0;
    uint8_t tickerLength;
    uint64_t chain_id;
    uint8_t hash[INT256_LENGTH];
    tokenDefinition_t *token = &get_current_asset_info()->token;

    PRINTF("Provisioning currentAssetIndex %d\n", tmpCtx.transactionContext.currentAssetIndex);

    if (dataLength < 1) {
        return SWO_INCORRECT_DATA;
    }
    tickerLength = workBuffer[offset++];
    dataLength--;
    if ((tickerLength + 1) > sizeof(token->ticker)) {
        return SWO_INCORRECT_DATA;
    }
    if (dataLength < tickerLength + 20 + 4 + 4) {
        return SWO_INCORRECT_DATA;
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
        return SWO_INCORRECT_DATA;
    }
    offset += 4;
    dataLength -= 4;

    if (check_signature_with_pubkey(hash,
                                    sizeof(hash),
                                    CERTIFICATE_PUBLIC_KEY_USAGE_COIN_META,
                                    (uint8_t *) (workBuffer + offset),
                                    dataLength) != true) {
        return SWO_INCORRECT_DATA;
    }

    G_io_apdu_buffer[0] = tmpCtx.transactionContext.currentAssetIndex;
    validate_current_asset_info();
    *tx += 1;
    return SWO_SUCCESS;
}
