#ifdef HAVE_NFT_SUPPORT

#include "shared_context.h"
#include "apdu_constants.h"
#include "tokens.h"
#include "utils.h"
#include "common_ui.h"
#include "os_io_seproxyhal.h"

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

#define TEST_NFT_METADATA_KEY 0
#define PROD_NFT_METADATA_KEY 1

#define ALGORITHM_ID_1 1

#define TYPE_1 1

#define VERSION_1 1

static const uint8_t LEDGER_NFT_METADATA_PUBLIC_KEY[] = {
#ifdef HAVE_NFT_TESTING_KEY
    0x04, 0xf5, 0x70, 0x0c, 0xa1, 0xe8, 0x74, 0x24, 0xc7, 0xc7, 0xd1, 0x19, 0xe7, 0xe3,
    0xc1, 0x89, 0xb1, 0x62, 0x50, 0x94, 0xdb, 0x6e, 0xa0, 0x40, 0x87, 0xc8, 0x30, 0x00,
    0x7d, 0x0b, 0x46, 0x9a, 0x53, 0x11, 0xee, 0x6a, 0x1a, 0xcd, 0x1d, 0xa5, 0xaa, 0xb0,
    0xf5, 0xc6, 0xdf, 0x13, 0x15, 0x8d, 0x28, 0xcc, 0x12, 0xd1, 0xdd, 0xa6, 0xec, 0xe9,
    0x46, 0xb8, 0x9d, 0x5c, 0x05, 0x49, 0x92, 0x59, 0xc4
#else
    0x04, 0x98, 0x8d, 0xa6, 0xb2, 0x46, 0xf2, 0x8e, 0x77, 0xc1, 0xba, 0xb6, 0x75, 0xcb,
    0x2a, 0x27, 0x44, 0xf7, 0xf5, 0xce, 0xc5, 0x6a, 0xe6, 0xe0, 0x32, 0x23, 0x33, 0x7b,
    0x57, 0x94, 0xcd, 0x6a, 0xe0, 0x7d, 0x48, 0xb3, 0x0d, 0xb9, 0xcc, 0xb4, 0x0f, 0x5a,
    0x02, 0xa1, 0x1a, 0x3a, 0xb9, 0x9d, 0x5f, 0x59, 0x5a, 0x3d, 0x50, 0xa0, 0xe1, 0x30,
    0x23, 0xfd, 0x0d, 0x95, 0x87, 0x92, 0xd7, 0x97, 0x01
#endif
};

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
        THROW(0x6A80);
    }

    uint8_t type = workBuffer[offset];
    switch (type) {
        case TYPE_1:
            break;
        default:
            PRINTF("Unsupported type %d\n", type);
            THROW(0x6a80);
            break;
    }
    offset += TYPE_SIZE;

    uint8_t version = workBuffer[offset];
    switch (version) {
        case VERSION_1:
            break;
        default:
            PRINTF("Unsupported version %d\n", version);
            THROW(0x6a80);
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
        THROW(0x6A80);
    }

    if (collectionNameLength > COLLECTION_NAME_MAX_LEN) {
        PRINTF("CollectionName too big: expected max %d, got %d\n",
               COLLECTION_NAME_MAX_LEN,
               collectionNameLength);
        THROW(0x6A80);
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

    uint64_t chainId = u64_from_BE(workBuffer + offset, CHAIN_ID_SIZE);
    // this prints raw data, so to have a more meaningful print, display
    // the buffer before the endianness swap
    PRINTF("ChainID: %.*H\n", sizeof(chainId), (workBuffer + offset));
    if ((chainConfig->chainId != 0) && (chainConfig->chainId != chainId)) {
        PRINTF("Chain ID token mismatch\n");
        THROW(0x6A80);
    }
    offset += CHAIN_ID_SIZE;

    uint8_t keyId = workBuffer[offset];
    uint8_t *rawKey;
    uint8_t rawKeyLen;

    PRINTF("KeyID: %d\n", keyId);
    switch (keyId) {
#ifdef HAVE_NFT_TESTING_KEY
        case TEST_NFT_METADATA_KEY:
#endif
        case PROD_NFT_METADATA_KEY:
            rawKey = (uint8_t *) LEDGER_NFT_METADATA_PUBLIC_KEY;
            rawKeyLen = sizeof(LEDGER_NFT_METADATA_PUBLIC_KEY);
            break;
        default:
            PRINTF("KeyID %d not supported\n", keyId);
            THROW(0x6A80);
            break;
    }
    PRINTF("RawKey: %.*H\n", rawKeyLen, rawKey);
    offset += KEY_ID_SIZE;

    uint8_t algorithmId = workBuffer[offset];
    PRINTF("Algorithm: %d\n", algorithmId);
    cx_curve_t curve;
    verificationAlgo *verificationFn;
    cx_md_t hashId;

    switch (algorithmId) {
        case ALGORITHM_ID_1:
            curve = CX_CURVE_256K1;
            verificationFn = (verificationAlgo *) cx_ecdsa_verify;
            hashId = CX_SHA256;
            break;
        default:
            PRINTF("Incorrect algorithmId %d\n", algorithmId);
            THROW(0x6a80);
            break;
    }
    offset += ALGORITHM_ID_SIZE;
    PRINTF("hashing: %.*H\n", payloadSize, workBuffer);
    cx_hash_sha256(workBuffer, payloadSize, hash, sizeof(hash));

    if (dataLength < payloadSize + SIGNATURE_LENGTH_SIZE) {
        PRINTF("Data too short to hold signature length\n");
        THROW(0x6a80);
    }

    uint8_t signatureLen = workBuffer[offset];
    PRINTF("Sigature len: %d\n", signatureLen);
    if (signatureLen < MIN_DER_SIG_SIZE || signatureLen > MAX_DER_SIG_SIZE) {
        PRINTF("SignatureLen too big or too small. Must be between %d and %d, got %d\n",
               MIN_DER_SIG_SIZE,
               MAX_DER_SIG_SIZE,
               signatureLen);
        THROW(0x6a80);
    }
    offset += SIGNATURE_LENGTH_SIZE;

    if (dataLength < payloadSize + SIGNATURE_LENGTH_SIZE + signatureLen) {
        PRINTF("Signature could not fit in data\n");
        THROW(0x6a80);
    }

    cx_ecfp_init_public_key(curve, rawKey, rawKeyLen, &nftKey);
    if (!verificationFn(&nftKey,
                        CX_LAST,
                        hashId,
                        hash,
                        sizeof(hash),
                        (uint8_t *) workBuffer + offset,
                        signatureLen)) {
#ifndef HAVE_BYPASS_SIGNATURES
        PRINTF("Invalid NFT signature\n");
        THROW(0x6A80);
#endif
    }

    tmpCtx.transactionContext.tokenSet[tmpCtx.transactionContext.currentItemIndex] = 1;
    THROW(0x9000);
}

#endif  // HAVE_NFT_SUPPORT
