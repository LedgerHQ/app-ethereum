#include "shared_context.h"
#include "apdu_constants.h"
#include "ui_flow.h"
#include "tokens.h"

#define TYPE_SIZE        1
#define VERSION_SIZE     1
#define NAME_LENGTH_SIZE 1
#define CHAINID_SIZE     8
#define SIGNATURE_SIZE   65

void handleProvideNFTInformation(uint8_t p1,
                                 uint8_t p2,
                                 uint8_t *workBuffer,
                                 uint16_t dataLength,
                                 unsigned int *flags,
                                 unsigned int *tx) {
    UNUSED(p1);
    UNUSED(p2);
    UNUSED(tx);
    UNUSED(flags);
    uint8_t hash[INT256_LENGTH];
    cx_ecfp_public_key_t nftKey;

    tmpCtx.transactionContext.currentTokenIndex =
        (tmpCtx.transactionContext.currentTokenIndex + 1) % MAX_TOKEN;
    nftInfo_t *nft = &tmpCtx.transactionContext.nfts[tmpCtx.transactionContext.currentTokenIndex];

    PRINTF("Provisioning currentTokenIndex %d\n", tmpCtx.transactionContext.currentTokenIndex);

    uint8_t size = TYPE_SIZE + VERSION_SIZE + NAME_LENGTH_SIZE;
    uint8_t offset = 0;

    if (dataLength <= size) {
        THROW(0x6A80);
    }

    uint8_t _type = workBuffer[offset];
    offset++;

    uint8_t _version = workBuffer[offset];
    offset++;

    uint8_t collectionNameLength = workBuffer[offset];
    offset++;

    if (dataLength < size + collectionNameLength + ADDRESS_LENGTH + CHAINID_SIZE + SIGNATURE_SIZE) {
        THROW(0x6A80);
    }

    if (collectionNameLength + 1 > sizeof(nft->nftCollectionName)) {
        THROW(0x6A80);
    }

    // Safe because we've checked the size before.
    memcpy(nft->nftCollectionName, workBuffer, collectionNameLength);
    nft->nftCollectionName[collectionNameLength] = '\0';

    cx_hash_sha256(workBuffer,
                   TYPE_SIZE + VERSION_SIZE + NAME_LENGTH_SIZE + collectionNameLength +
                       ADDRESS_LENGTH + CHAINID_SIZE,
                   hash,
                   sizeof(hash));

    offset += collectionNameLength;
    memcpy(nft->contractAddress, workBuffer + offset, ADDRESS_LENGTH);

    offset += ADDRESS_LENGTH;

    // TODO: scott make something with chainID and assert in tx.
    memcpy(NULL, workBuffer + offset, CHAINID_SIZE);
    offset += CHAINID_SIZE;

    cx_ecfp_init_public_key(CX_CURVE_256K1,
                            LEDGER_SIGNATURE_PUBLIC_KEY,
                            sizeof(LEDGER_SIGNATURE_PUBLIC_KEY),
                            &nftKey);
    if (!cx_ecdsa_verify(&nftKey,
                         CX_LAST,
                         CX_SHA256,
                         hash,
                         sizeof(hash),
                         workBuffer + offset,
                         SIGNATURE_SIZE)) {
#ifndef HAVE_BYPASS_SIGNATURES
        PRINTF("Invalid NFT signature\n");
        THROW(0x6A80);
#endif
    }

    tmpCtx.transactionContext.tokenSet[tmpCtx.transactionContext.currentTokenIndex] = 1;
    THROW(0x9000);
}