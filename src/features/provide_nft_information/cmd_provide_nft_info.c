#include "apdu_constants.h"
#include "asset_info.h"
#include "network.h"
#include "public_keys.h"
#include "manage_asset_info.h"
#include "os_pki.h"

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

/**
 * Handle the APDU command that provides NFT collection metadata to the app.
 *
 * The APDU payload is expected to contain a small header (type, version, and
 * collection name length), followed by the collection name and the chain
 * identifier on which the NFT collection exists. The payload also carries
 * information about which Ledger-controlled public key and algorithm were
 * used to sign the metadata, as well as a DER-encoded signature.
 *
 * High-level flow:
 * - Parse and validate the header fields (type, version, name length).
 * - Read the collection name and the chain ID from the work buffer.
 * - Extract the key identifier, algorithm identifier, and the length of the
 *   attached DER-encoded signature.
 * - Compute a hash over the structured NFT metadata payload.
 * - Verify the signature against the appropriate Ledger NFT metadata key
 *   (staging or production, depending on the build configuration).
 * - If verification succeeds, store the parsed NFT metadata in app state so
 *   that subsequent commands (for example, transaction signing) can display
 *   or otherwise use the NFT collection information.
 *
 * @param workBuffer  Pointer to the APDU data buffer containing the NFT
 *                    metadata payload to be parsed and verified.
 * @param dataLength  Length of the data available in workBuffer.
 * @param tx          Pointer to the variable that will hold the number of
 *                    bytes written to the APDU response buffer.
 *
 * @return SW_OK on success, or an ISO7816-style status word describing the
 *         validation or parsing error encountered.
 */
uint16_t handle_provide_nft_information(const uint8_t *workBuffer,
                                        uint8_t dataLength,
                                        unsigned int *tx) {
    uint8_t hash[INT256_LENGTH];
    nftInfo_t *nft = NULL;
    size_t offset = 0;
    size_t payloadSize = 0;
    uint8_t collectionNameLength = 0;
    uint64_t chain_id = 0;
    uint8_t signatureLen = 0;
#ifdef HAVE_NFT_STAGING_KEY
    uint8_t valid_keyId = STAGING_NFT_METADATA_KEY;
#else
    uint8_t valid_keyId = PROD_NFT_METADATA_KEY;
#endif

    PRINTF("In handle provide NFTInformation\n");

    // Retrieve the NFT sub-structure from the current asset info slot.
    nft = &get_current_asset_info()->nft;

    PRINTF("Provisioning currentAssetIndex %d\n", tmpCtx.transactionContext.currentAssetIndex);

    // --- Header validation ---
    // The buffer must be large enough to hold at least the fixed-size header
    // (type + version + collection name length).
    if (dataLength <= HEADER_SIZE) {
        PRINTF("Data too small for headers: expected at least %d, got %d\n",
               HEADER_SIZE,
               dataLength);
        return SWO_INCORRECT_DATA;
    }

    // Reject unknown structure types so that future formats are not silently
    // mis-parsed by this implementation.
    if (workBuffer[offset] != TYPE_1) {
        PRINTF("Unsupported type %d\n", workBuffer[offset]);
        return SWO_INCORRECT_DATA;
    }
    offset += TYPE_SIZE;

    // Reject unknown versions for the same reason.
    if (workBuffer[offset] != VERSION_1) {
        PRINTF("Unsupported version %d\n", workBuffer[offset]);
        return SWO_INCORRECT_DATA;
    }
    offset += VERSION_SIZE;

    // Read the collection name length declared in the header.
    collectionNameLength = workBuffer[offset];
    offset += NAME_LENGTH_SIZE;

    // --- Payload size validation ---
    // Size of the payload (everything except the signature).
    // This is computed now that the variable-length collection name size is known.
    payloadSize = HEADER_SIZE + collectionNameLength + ADDRESS_LENGTH + CHAIN_ID_SIZE +
                  KEY_ID_SIZE + ALGORITHM_ID_SIZE;
    if (dataLength < payloadSize) {
        PRINTF("Data too small for payload: expected at least %d, got %d\n",
               payloadSize,
               dataLength);
        return SWO_INCORRECT_DATA;
    }

    // Guard against a collection name that would overflow the destination buffer.
    if (collectionNameLength > COLLECTION_NAME_MAX_LEN) {
        PRINTF("CollectionName too big: expected max %d, got %d\n",
               COLLECTION_NAME_MAX_LEN,
               collectionNameLength);
        return SWO_INCORRECT_DATA;
    }

    // --- Collection name parsing ---
    // Safe because we've checked the size before.
    memcpy(nft->collectionName, workBuffer + offset, collectionNameLength);
    nft->collectionName[collectionNameLength] = '\0';

    PRINTF("Length: %d\n", collectionNameLength);
    PRINTF("CollectionName: %s\n", nft->collectionName);
    offset += collectionNameLength;

    // --- Contract address parsing ---
    memcpy(nft->contractAddress, workBuffer + offset, ADDRESS_LENGTH);
    PRINTF("Address: %.*H\n", ADDRESS_LENGTH, workBuffer + offset);
    offset += ADDRESS_LENGTH;

    // --- Chain ID parsing and compatibility check ---
    // The chain ID is encoded as a big-endian 64-bit integer.
    // Reject it if the running app was built for a different chain.
    chain_id = u64_from_BE(workBuffer + offset, CHAIN_ID_SIZE);
    PRINTF("ChainID: %llu\n", chain_id);
    if (!app_compatible_with_chain_id(&chain_id)) {
        UNSUPPORTED_CHAIN_ID_MSG(chain_id);
        return SWO_INCORRECT_DATA;
    }
    offset += CHAIN_ID_SIZE;

    // --- Key ID validation ---
    // Depending on the build configuration (staging vs. production), only one
    // specific key identifier is accepted, ensuring the correct trust anchor is used.
    if (workBuffer[offset] != valid_keyId) {
        PRINTF("Unsupported KeyID %d\n", workBuffer[offset]);
        return SWO_INCORRECT_DATA;
    }
    offset += KEY_ID_SIZE;

    // --- Algorithm ID validation ---
    // Only algorithm ID 1 (ECDSA over secp256k1 with SHA-256) is supported.
    if (workBuffer[offset] != ALGORITHM_ID_1) {
        PRINTF("Incorrect algorithmId %d\n", workBuffer[offset]);
        return SWO_INCORRECT_DATA;
    }
    offset += ALGORITHM_ID_SIZE;

    // --- Payload hashing ---
    // Hash the structured payload (header through algorithm ID) so that the
    // signature can be verified against a fixed-size digest.
    PRINTF("hashing: %.*H\n", payloadSize, workBuffer);
    cx_hash_sha256(workBuffer, payloadSize, hash, sizeof(hash));

    // --- Signature length parsing and validation ---
    if (dataLength < payloadSize + SIGNATURE_LENGTH_SIZE) {
        PRINTF("Data too short to hold signature length\n");
        return SWO_INCORRECT_DATA;
    }

    signatureLen = workBuffer[offset];
    PRINTF("Signature len: %d\n", signatureLen);
    // DER-encoded ECDSA signatures over a 256-bit curve fall within a known size range.
    if (signatureLen < MIN_DER_SIG_SIZE || signatureLen > MAX_DER_SIG_SIZE) {
        PRINTF("SignatureLen too big or too small. Must be between %d and %d, got %d\n",
               MIN_DER_SIG_SIZE,
               MAX_DER_SIG_SIZE,
               signatureLen);
        return SWO_INCORRECT_DATA;
    }
    offset += SIGNATURE_LENGTH_SIZE;

    // Verify that the declared signature bytes are actually present in the buffer.
    if (dataLength < payloadSize + SIGNATURE_LENGTH_SIZE + signatureLen) {
        PRINTF("Signature could not fit in data\n");
        return SWO_INCORRECT_DATA;
    }

    // --- Signature verification ---
    // Verify the DER signature against the Ledger NFT metadata public key.
    // Reject the payload if the signature is invalid.
    if (check_signature_with_pubkey(hash,
                                    sizeof(hash),
                                    LEDGER_NFT_METADATA_PUBLIC_KEY,
                                    sizeof(LEDGER_NFT_METADATA_PUBLIC_KEY),
                                    CERTIFICATE_PUBLIC_KEY_USAGE_NFT_METADATA,
                                    (uint8_t *) (workBuffer + offset),
                                    signatureLen) != true) {
        return SWO_INCORRECT_DATA;
    }

    // --- Commit the validated metadata ---
    // Write the asset index into the response buffer, mark the asset info as
    // validated, and advance the response length by one byte.
    G_io_tx_buffer[0] = tmpCtx.transactionContext.currentAssetIndex;
    validate_current_asset_info();
    *tx += 1;
    return SWO_SUCCESS;
}
