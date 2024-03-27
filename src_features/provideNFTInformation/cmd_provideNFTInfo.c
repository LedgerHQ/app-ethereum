#ifdef HAVE_NFT_SUPPORT

#include "shared_context.h"
#include "apdu_constants.h"
#include "asset_info.h"
#include "common_utils.h"
#include "common_ui.h"
#include "os_io_seproxyhal.h"
#include "network.h"
#include "public_keys.h"

#define TYPE_SIZE        1
#define VERSION_SIZE     1
#define NAME_LENGTH_SIZE 1
#define HEADER_SIZE      (TYPE_SIZE + VERSION_SIZE + NAME_LENGTH_SIZE)

#define CHAIN_ID_SIZE         8
#define KEY_ID_SIZE           1
#define ALGORITHM_ID_SIZE     1
#define SIGNATURE_LENGTH_SIZE 1
#define MIN_DER_SIG_SIZE      67
#define MAX_DER_SIG_SIZE      72

#define STAGING_NFT_METADATA_KEY 0
#define PROD_NFT_METADATA_KEY    1

#define ALGORITHM_ID_1 1

#define TYPE_1 1

#define VERSION_1 1

typedef bool verificationAlgo(const cx_ecfp_public_key_t *,
                              int,
                              cx_md_t,
                              const unsigned char *,
                              unsigned int,
                              unsigned char *,
                              unsigned int);

void handleProvideNFTInformation(uint8_t p1,
                                 uint8_t p2,
                                 const uint8_t *workBuffer,
                                 uint8_t dataLength,
                                 unsigned int *flags,
                                 unsigned int *tx) {
    UNUSED(p1);
    UNUSED(p2);
    UNUSED(tx);
    UNUSED(flags);
    uint8_t hash[INT256_LENGTH];
    cx_ecfp_public_key_t nftKey;
    PRINTF("In handle provide NFTInformation\n");

    if ((pluginType != ERC721) && (pluginType != ERC1155)) {
        PRINTF("NFT metadata provided without proper plugin loaded!\n");
        THROW(0x6985);
    }
    tmpCtx.transactionContext.currentItemIndex =
        (tmpCtx.transactionContext.currentItemIndex + 1) % MAX_ITEMS;
    nftInfo_t *nft =
        &tmpCtx.transactionContext.extraInfo[tmpCtx.transactionContext.currentItemIndex].nft;

    PRINTF("Provisioning currentItemIndex %d\n", tmpCtx.transactionContext.currentItemIndex);

    size_t offset = 0;

    if (dataLength <= HEADER_SIZE) {
        PRINTF("Data too small for headers: expected at least %d, got %d\n",
               HEADER_SIZE,
               dataLength);
        THROW(APDU_RESPONSE_INVALID_DATA);
    }

    uint8_t type = workBuffer[offset];
    switch (type) {
        case TYPE_1:
            break;
        default:
            PRINTF("Unsupported type %d\n", type);
            THROW(APDU_RESPONSE_INVALID_DATA);
            break;
    }
    offset += TYPE_SIZE;

    uint8_t version = workBuffer[offset];
    switch (version) {
        case VERSION_1:
            break;
        default:
            PRINTF("Unsupported version %d\n", version);
            THROW(APDU_RESPONSE_INVALID_DATA);
            break;
    }
    offset += VERSION_SIZE;

    uint8_t collectionNameLength = workBuffer[offset];
    offset += NAME_LENGTH_SIZE;

    // Size of the payload (everything except the signature)
    size_t payloadSize = HEADER_SIZE + collectionNameLength + ADDRESS_LENGTH + CHAIN_ID_SIZE +
                         KEY_ID_SIZE + ALGORITHM_ID_SIZE;
    if (dataLength < payloadSize) {
        PRINTF("Data too small for payload: expected at least %d, got %d\n",
               payloadSize,
               dataLength);
        THROW(APDU_RESPONSE_INVALID_DATA);
    }

    if (collectionNameLength > COLLECTION_NAME_MAX_LEN) {
        PRINTF("CollectionName too big: expected max %d, got %d\n",
               COLLECTION_NAME_MAX_LEN,
               collectionNameLength);
        THROW(APDU_RESPONSE_INVALID_DATA);
    }

    // Safe because we've checked the size before.
    memcpy(nft->collectionName, workBuffer + offset, collectionNameLength);
    nft->collectionName[collectionNameLength] = '\0';

    PRINTF("Length: %d\n", collectionNameLength);
    PRINTF("CollectionName: %s\n", nft->collectionName);
    offset += collectionNameLength;

    memcpy(nft->contractAddress, workBuffer + offset, ADDRESS_LENGTH);
    PRINTF("Address: %.*H\n", ADDRESS_LENGTH, workBuffer + offset);
    offset += ADDRESS_LENGTH;

    uint64_t chain_id = u64_from_BE(workBuffer + offset, CHAIN_ID_SIZE);
    // this prints raw data, so to have a more meaningful print, display
    // the buffer before the endianness swap
    PRINTF("ChainID: %.*H\n", sizeof(chain_id), (workBuffer + offset));
    if (!app_compatible_with_chain_id(&chain_id)) {
        UNSUPPORTED_CHAIN_ID_MSG(chain_id);
        THROW(APDU_RESPONSE_INVALID_DATA);
    }
    offset += CHAIN_ID_SIZE;

    uint8_t keyId = workBuffer[offset];
    uint8_t *rawKey;
    uint8_t rawKeyLen;

    PRINTF("KeyID: %d\n", keyId);
    switch (keyId) {
#ifdef HAVE_NFT_STAGING_KEY
        case STAGING_NFT_METADATA_KEY:
#endif
        case PROD_NFT_METADATA_KEY:
            rawKey = (uint8_t *) LEDGER_NFT_METADATA_PUBLIC_KEY;
            rawKeyLen = sizeof(LEDGER_NFT_METADATA_PUBLIC_KEY);
            break;
        default:
            PRINTF("KeyID %d not supported\n", keyId);
            THROW(APDU_RESPONSE_INVALID_DATA);
            break;
    }
    PRINTF("RawKey: %.*H\n", rawKeyLen, rawKey);
    offset += KEY_ID_SIZE;

    uint8_t algorithmId = workBuffer[offset];
    PRINTF("Algorithm: %d\n", algorithmId);

    if (algorithmId != ALGORITHM_ID_1) {
        PRINTF("Incorrect algorithmId %d\n", algorithmId);
        THROW(APDU_RESPONSE_INVALID_DATA);
    }
    offset += ALGORITHM_ID_SIZE;
    PRINTF("hashing: %.*H\n", payloadSize, workBuffer);
    cx_hash_sha256(workBuffer, payloadSize, hash, sizeof(hash));

    if (dataLength < payloadSize + SIGNATURE_LENGTH_SIZE) {
        PRINTF("Data too short to hold signature length\n");
        THROW(APDU_RESPONSE_INVALID_DATA);
    }

    uint8_t signatureLen = workBuffer[offset];
    PRINTF("Signature len: %d\n", signatureLen);
    if (signatureLen < MIN_DER_SIG_SIZE || signatureLen > MAX_DER_SIG_SIZE) {
        PRINTF("SignatureLen too big or too small. Must be between %d and %d, got %d\n",
               MIN_DER_SIG_SIZE,
               MAX_DER_SIG_SIZE,
               signatureLen);
        THROW(APDU_RESPONSE_INVALID_DATA);
    }
    offset += SIGNATURE_LENGTH_SIZE;

    if (dataLength < payloadSize + SIGNATURE_LENGTH_SIZE + signatureLen) {
        PRINTF("Signature could not fit in data\n");
        THROW(APDU_RESPONSE_INVALID_DATA);
    }

    CX_ASSERT(cx_ecfp_init_public_key_no_throw(CX_CURVE_256K1, rawKey, rawKeyLen, &nftKey));
    if (!cx_ecdsa_verify_no_throw(&nftKey,
                                  hash,
                                  sizeof(hash),
                                  (uint8_t *) workBuffer + offset,
                                  signatureLen)) {
#ifndef HAVE_BYPASS_SIGNATURES
        PRINTF("Invalid NFT signature\n");
        THROW(APDU_RESPONSE_INVALID_DATA);
#endif
    }

    tmpCtx.transactionContext.tokenSet[tmpCtx.transactionContext.currentItemIndex] = 1;
    THROW(0x9000);
}

#endif  // HAVE_NFT_SUPPORT
