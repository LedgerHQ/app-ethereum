#include "shared_context.h"
#include "apdu_constants.h"
#include "public_keys.h"
#include "common_ui.h"
#include "os_io_seproxyhal.h"
#include "extra_tokens.h"
#include "network.h"
#include "manage_asset_info.h"

#ifdef HAVE_CONTRACT_NAME_IN_DESCRIPTOR

void handleProvideErc20TokenInformation(uint8_t p1,
                                        uint8_t p2,
                                        const uint8_t *workBuffer,
                                        uint8_t dataLength,
                                        unsigned int *flags,
                                        unsigned int *tx) {
    UNUSED(p1);
    UNUSED(p2);
    UNUSED(flags);
    uint32_t offset = 0;
    uint8_t tickerLength, contractNameLength;
    uint32_t chainId;
    uint8_t hash[INT256_LENGTH];
    cx_sha256_t sha256;
    cx_ecfp_public_key_t tokenKey;

    cx_sha256_init(&sha256);

    tokenDefinition_t *token = &get_current_asset_info()->token;

    if (dataLength < 1) {
        THROW(0x6A80);
    }
    tickerLength = workBuffer[offset++];
    dataLength--;
    if ((tickerLength + 2) >= sizeof(token->ticker)) {  // +2 because ' \0' is appended to ticker
        THROW(0x6A80);
    }
    if (dataLength < tickerLength + 1) {
        THROW(0x6A80);
    }
    cx_hash((cx_hash_t *) &sha256, 0, workBuffer + offset, tickerLength, NULL, 0);
    memmove(token->ticker, workBuffer + offset, tickerLength);
    token->ticker[tickerLength] = '\0';
    offset += tickerLength;
    dataLength -= tickerLength;

    contractNameLength = workBuffer[offset++];
    dataLength--;
    if (dataLength < contractNameLength + 20 + 4 + 4) {
        THROW(0x6A80);
    }
    cx_hash((cx_hash_t *) &sha256,
            CX_LAST,
            workBuffer + offset,
            contractNameLength + 20 + 4 + 4,
            hash,
            32);
    memmove(token->contractName,
            workBuffer + offset,
            MIN(contractNameLength, sizeof(token->contractName) - 1));
    token->contractName[MIN(contractNameLength, sizeof(token->contractName) - 1)] = '\0';
    offset += contractNameLength;
    dataLength -= contractNameLength;

    memmove(token->address, workBuffer + offset, 20);
    offset += 20;
    dataLength -= 20;
    token->decimals = U4BE(workBuffer, offset);
    offset += 4;
    dataLength -= 4;
    chainId = U4BE(workBuffer, offset);
    if ((chainConfig->chainId != 0) && (chainConfig->chainId != chainId)) {
        PRINTF("ChainId token mismatch\n");
        THROW(0x6A80);
    }
    offset += 4;
    dataLength -= 4;
    cx_ecfp_init_public_key(CX_CURVE_256K1,
                            LEDGER_SIGNATURE_PUBLIC_KEY,
                            sizeof(LEDGER_SIGNATURE_PUBLIC_KEY),
                            &tokenKey);
    if (!cx_ecdsa_verify(&tokenKey,
                         CX_LAST,
                         CX_SHA256,
                         hash,
                         32,
                         workBuffer + offset,
                         dataLength)) {
#ifndef HAVE_BYPASS_SIGNATURES
        PRINTF("Invalid token signature\n");
        THROW(0x6A80);
#endif
    }
    validate_current_asset_info();
    THROW(0x9000);
}

#else

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

#ifdef HAVE_TOKENS_EXTRA_LIST
    tokenDefinition_t *currentToken = NULL;
    uint32_t index;
    for (index = 0; index < NUM_TOKENS_EXTRA; index++) {
        currentToken = (tokenDefinition_t *) PIC(&TOKENS_EXTRA[index]);
        if (memcmp(currentToken->address, token->address, 20) == 0) {
            strcpy((char *) token->ticker, (char *) currentToken->ticker);
            token->decimals = currentToken->decimals;
            break;
        }
    }
    if (index < NUM_TOKENS_EXTRA) {
        PRINTF("Descriptor whitelisted\n");
    } else
#endif
    {
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
    }

    validate_current_asset_info();
    THROW(0x9000);
}

#endif
