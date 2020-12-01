#include "shared_context.h"
#include "apdu_constants.h"
#include "ui_flow.h"

static const uint8_t const TOKEN_SIGNATURE_PUBLIC_KEY[] = {
    // production key 2019-01-11 03:07PM (erc20signer)
    0x04,

    0x5e, 0x6c, 0x10, 0x20, 0xc1, 0x4d, 0xc4, 0x64, 0x42, 0xfe, 0x89, 0xf9, 0x7c, 0x0b, 0x68, 0xcd,
    0xb1, 0x59, 0x76, 0xdc, 0x24, 0xf2, 0x4c, 0x31, 0x6e, 0x7b, 0x30, 0xfe, 0x4e, 0x8c, 0xc7, 0x6b,

    0x14, 0x89, 0x15, 0x0c, 0x21, 0x51, 0x4e, 0xbf, 0x44, 0x0f, 0xf5, 0xde, 0xa5, 0x39, 0x3d, 0x83,
    0xde, 0x53, 0x58, 0xcd, 0x09, 0x8f, 0xce, 0x8f, 0xd0, 0xf8, 0x1d, 0xaa, 0x94, 0x97, 0x91, 0x83};

#ifdef HAVE_CONTRACT_NAME_IN_DESCRIPTOR

void handleProvideErc20TokenInformation(uint8_t p1,
                                        uint8_t p2,
                                        uint8_t *workBuffer,
                                        uint16_t dataLength,
                                        unsigned int *flags,
                                        unsigned int *tx) {
    UNUSED(p1);
    UNUSED(p2);
    UNUSED(flags);
    uint32_t offset = 0;
    uint8_t tickerLength, contractNameLength;
    uint32_t chainId;
    uint8_t hash[32];
    cx_sha256_t sha256;
    cx_ecfp_public_key_t tokenKey;

    cx_sha256_init(&sha256);

    tmpCtx.transactionContext.currentTokenIndex =
        (tmpCtx.transactionContext.currentTokenIndex + 1) % MAX_TOKEN;
    tokenDefinition_t *token =
        &tmpCtx.transactionContext.tokens[tmpCtx.transactionContext.currentTokenIndex];

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
    token->ticker[tickerLength] = ' ';
    token->ticker[tickerLength + 1] = '\0';
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
                            TOKEN_SIGNATURE_PUBLIC_KEY,
                            sizeof(TOKEN_SIGNATURE_PUBLIC_KEY),
                            &tokenKey);
    if (!cx_ecdsa_verify(&tokenKey,
                         CX_LAST,
                         CX_SHA256,
                         hash,
                         32,
                         workBuffer + offset,
                         dataLength)) {
        PRINTF("Invalid token signature\n");
        THROW(0x6A80);
    }
    tmpCtx.transactionContext.tokenSet[tmpCtx.transactionContext.currentTokenIndex] = 1;
    THROW(0x9000);
}

#else

void handleProvideErc20TokenInformation(uint8_t p1,
                                        uint8_t p2,
                                        uint8_t *workBuffer,
                                        uint16_t dataLength,
                                        unsigned int *flags,
                                        unsigned int *tx) {
    UNUSED(p1);
    UNUSED(p2);
    UNUSED(flags);
    uint32_t offset = 0;
    uint8_t tickerLength;
    uint32_t chainId;
    uint8_t hash[32];
    cx_ecfp_public_key_t tokenKey;

    tmpCtx.transactionContext.currentTokenIndex =
        (tmpCtx.transactionContext.currentTokenIndex + 1) % MAX_TOKEN;
    tokenDefinition_t *token =
        &tmpCtx.transactionContext.tokens[tmpCtx.transactionContext.currentTokenIndex];

    PRINTF("Provisioning currentTokenIndex %d\n", tmpCtx.transactionContext.currentTokenIndex);

    if (dataLength < 1) {
        THROW(0x6A80);
    }
    tickerLength = workBuffer[offset++];
    dataLength--;
    if ((tickerLength + 1) >= sizeof(token->ticker)) {
        THROW(0x6A80);
    }
    if (dataLength < tickerLength + 20 + 4 + 4) {
        THROW(0x6A80);
    }
    cx_hash_sha256(workBuffer + offset, tickerLength + 20 + 4 + 4, hash, 32);
    memmove(token->ticker, workBuffer + offset, tickerLength);
    token->ticker[tickerLength] = ' ';
    token->ticker[tickerLength + 1] = '\0';
    offset += tickerLength;
    dataLength -= tickerLength;
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
    } else {
        cx_ecfp_init_public_key(CX_CURVE_256K1,
                                TOKEN_SIGNATURE_PUBLIC_KEY,
                                sizeof(TOKEN_SIGNATURE_PUBLIC_KEY),
                                &tokenKey);
        if (!cx_ecdsa_verify(&tokenKey,
                             CX_LAST,
                             CX_SHA256,
                             hash,
                             32,
                             workBuffer + offset,
                             dataLength)) {
            PRINTF("Invalid token signature\n");
            THROW(0x6A80);
        }
    }

#else

    cx_ecfp_init_public_key(CX_CURVE_256K1,
                            TOKEN_SIGNATURE_PUBLIC_KEY,
                            sizeof(TOKEN_SIGNATURE_PUBLIC_KEY),
                            &tokenKey);
    if (!cx_ecdsa_verify(&tokenKey,
                         CX_LAST,
                         CX_SHA256,
                         hash,
                         32,
                         workBuffer + offset,
                         dataLength)) {
        PRINTF("Invalid token signature\n");
        THROW(0x6A80);
    }
#endif

    tmpCtx.transactionContext.tokenSet[tmpCtx.transactionContext.currentTokenIndex] = 1;
    THROW(0x9000);
}

#endif
