#ifdef HAVE_NFT_SUPPORT

#include "shared_context.h"
#include "apdu_constants.h"
#include "asset_info.h"
#include "common_utils.h"
#include "common_ui.h"
#include "os_io_seproxyhal.h"
#include "network.h"
#include "public_keys.h"
#include "manage_asset_info.h"
#ifdef HAVE_LEDGER_PKI
#include "os_pki.h"
#endif

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

uint16_t handleProvideNFTInformation(const uint8_t *workBuffer,
                                     uint8_t dataLength,
                                     unsigned int *tx) {
    uint8_t hash[INT256_LENGTH];
    nftInfo_t *nft = NULL;
    size_t offset = 0;
    size_t payloadSize = 0;
    uint8_t collectionNameLength = 0;
    uint64_t chain_id = 0;
    uint8_t signatureLen = 0;
    cx_err_t error = CX_INTERNAL_ERROR;
#ifdef HAVE_NFT_STAGING_KEY
    uint8_t valid_keyId = STAGING_NFT_METADATA_KEY;
#else
    uint8_t valid_keyId = PROD_NFT_METADATA_KEY;
#endif

    PRINTF("In handle provide NFTInformation\n");

    nft = &get_current_asset_info()->nft;

    PRINTF("Provisioning currentAssetIndex %d\n", tmpCtx.transactionContext.currentAssetIndex);

    if (dataLength <= HEADER_SIZE) {
        PRINTF("Data too small for headers: expected at least %d, got %d\n",
               HEADER_SIZE,
               dataLength);
        return APDU_RESPONSE_INVALID_DATA;
    }

    if (workBuffer[offset] != TYPE_1) {
        PRINTF("Unsupported type %d\n", workBuffer[offset]);
        return APDU_RESPONSE_INVALID_DATA;
    }
    offset += TYPE_SIZE;

    if (workBuffer[offset] != VERSION_1) {
        PRINTF("Unsupported version %d\n", workBuffer[offset]);
        return APDU_RESPONSE_INVALID_DATA;
    }
    offset += VERSION_SIZE;

    collectionNameLength = workBuffer[offset];
    offset += NAME_LENGTH_SIZE;

    // Size of the payload (everything except the signature)
    payloadSize = HEADER_SIZE + collectionNameLength + ADDRESS_LENGTH + CHAIN_ID_SIZE +
                  KEY_ID_SIZE + ALGORITHM_ID_SIZE;
    if (dataLength < payloadSize) {
        PRINTF("Data too small for payload: expected at least %d, got %d\n",
               payloadSize,
               dataLength);
        return APDU_RESPONSE_INVALID_DATA;
    }

    if (collectionNameLength > COLLECTION_NAME_MAX_LEN) {
        PRINTF("CollectionName too big: expected max %d, got %d\n",
               COLLECTION_NAME_MAX_LEN,
               collectionNameLength);
        return APDU_RESPONSE_INVALID_DATA;
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

    chain_id = u64_from_BE(workBuffer + offset, CHAIN_ID_SIZE);
    // this prints raw data, so to have a more meaningful print, display
    // the buffer before the endianness swap
    PRINTF("ChainID: %.*H\n", sizeof(chain_id), (workBuffer + offset));
    if (!app_compatible_with_chain_id(&chain_id)) {
        UNSUPPORTED_CHAIN_ID_MSG(chain_id);
        return APDU_RESPONSE_INVALID_DATA;
    }
    offset += CHAIN_ID_SIZE;

    if (workBuffer[offset] != valid_keyId) {
        PRINTF("Unsupported KeyID %d\n", workBuffer[offset]);
        return APDU_RESPONSE_INVALID_DATA;
    }
    offset += KEY_ID_SIZE;

    if (workBuffer[offset] != ALGORITHM_ID_1) {
        PRINTF("Incorrect algorithmId %d\n", workBuffer[offset]);
        return APDU_RESPONSE_INVALID_DATA;
    }
    offset += ALGORITHM_ID_SIZE;

    PRINTF("hashing: %.*H\n", payloadSize, workBuffer);
    cx_hash_sha256(workBuffer, payloadSize, hash, sizeof(hash));

    if (dataLength < payloadSize + SIGNATURE_LENGTH_SIZE) {
        PRINTF("Data too short to hold signature length\n");
        return APDU_RESPONSE_INVALID_DATA;
    }

    signatureLen = workBuffer[offset];
    PRINTF("Signature len: %d\n", signatureLen);
    if (signatureLen < MIN_DER_SIG_SIZE || signatureLen > MAX_DER_SIG_SIZE) {
        PRINTF("SignatureLen too big or too small. Must be between %d and %d, got %d\n",
               MIN_DER_SIG_SIZE,
               MAX_DER_SIG_SIZE,
               signatureLen);
        return APDU_RESPONSE_INVALID_DATA;
    }
    offset += SIGNATURE_LENGTH_SIZE;

    if (dataLength < payloadSize + SIGNATURE_LENGTH_SIZE + signatureLen) {
        PRINTF("Signature could not fit in data\n");
        return APDU_RESPONSE_INVALID_DATA;
    }

    error = check_signature_with_pubkey("NFT Info",
                                        hash,
                                        sizeof(hash),
                                        LEDGER_NFT_METADATA_PUBLIC_KEY,
                                        sizeof(LEDGER_NFT_METADATA_PUBLIC_KEY),
#ifdef HAVE_LEDGER_PKI
                                        CERTIFICATE_PUBLIC_KEY_USAGE_NFT_METADATA,
#endif
                                        (uint8_t *) (workBuffer + offset),
                                        signatureLen);
#ifndef HAVE_BYPASS_SIGNATURES
    if (error != CX_OK) {
        return APDU_RESPONSE_INVALID_DATA;
    }
#endif

    G_io_apdu_buffer[0] = tmpCtx.transactionContext.currentAssetIndex;
    validate_current_asset_info();
    *tx += 1;
    return APDU_RESPONSE_OK;
}

#endif  // HAVE_NFT_SUPPORT
