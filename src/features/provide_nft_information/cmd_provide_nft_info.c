#include "read.h"
#include "apdu_constants.h"
#include "network.h"
#include "public_keys.h"
#include "os_pki.h"
#include "nft_info.h"

#define STAGING_NFT_METADATA_KEY 0
#define PROD_NFT_METADATA_KEY    1
#ifdef HAVE_NFT_STAGING_KEY
#define KEY_ID STAGING_NFT_METADATA_KEY
#else
#define KEY_ID PROD_NFT_METADATA_KEY
#endif

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
 * @param lc  Length of the data available in payload.
 * @param data  Pointer to the APDU data buffer containing the NFT
 *                    metadata payload to be parsed and verified.
 * @param tx          Pointer to the variable that will hold the number of
 *                    bytes written to the APDU response buffer.
 *
 * @return SW_OK on success, or an ISO7816-style status word describing the
 *         validation or parsing error encountered.
 */
uint16_t handle_provide_nft_information(uint8_t p1,
                                        uint8_t p2,
                                        uint8_t lc,
                                        const uint8_t *data,
                                        unsigned int *tx) {
    s_nft_info info = {0};
    uint8_t hash[CX_SHA256_SIZE];
    size_t offset = 0;
    uint8_t type;
    uint8_t version;
    uint8_t collection_name_length;
    uint8_t key_id;
    uint8_t algo_id;
    uint8_t sig_length;
    const uint8_t *sig;
    int index;

    if ((p1 != 0) || (p2 != 0)) {
        return SWO_INCORRECT_P1_P2;
    }

    if ((offset + sizeof(type)) > lc) {
        return SWO_INCORRECT_DATA;
    }
    type = data[offset];
    if (type != TYPE_1) {
        return SWO_INCORRECT_DATA;
    }
    offset += sizeof(type);

    if ((offset + sizeof(version)) > lc) {
        return SWO_INCORRECT_DATA;
    }
    version = data[offset];
    if (version != VERSION_1) {
        return SWO_INCORRECT_DATA;
    }
    offset += sizeof(version);

    if ((offset + sizeof(collection_name_length)) > lc) {
        return SWO_INCORRECT_DATA;
    }
    collection_name_length = data[offset];
    if ((size_t) collection_name_length + 1 > sizeof(info.collection_name)) {
        return SWO_INCORRECT_DATA;
    }
    offset += sizeof(collection_name_length);

    if ((offset + collection_name_length) > lc) {
        return SWO_INCORRECT_DATA;
    }
    memcpy(info.collection_name, &data[offset], collection_name_length);
    offset += collection_name_length;

    if ((offset + sizeof(info.address)) > lc) {
        return SWO_INCORRECT_DATA;
    }
    memcpy(info.address, &data[offset], sizeof(info.address));
    offset += sizeof(info.address);

    if ((offset + sizeof(info.chain_id)) > lc) {
        return SWO_INCORRECT_DATA;
    }
    info.chain_id = read_u64_be(data, offset);
    // Reject it if this app could never sign a transaction on that chain.
    if (!chain_id_is_signable(info.chain_id)) {
        UNSUPPORTED_CHAIN_ID_MSG(info.chain_id);
        return SWO_INCORRECT_DATA;
    }
    offset += sizeof(info.chain_id);

    if ((offset + sizeof(key_id)) > lc) {
        return SWO_INCORRECT_DATA;
    }
    key_id = data[offset];
    if (key_id != KEY_ID) {
        return SWO_INCORRECT_DATA;
    }
    offset += sizeof(key_id);

    if ((offset + sizeof(algo_id)) > lc) {
        return SWO_INCORRECT_DATA;
    }
    algo_id = data[offset];
    if (algo_id != ALGORITHM_ID_1) {
        return SWO_INCORRECT_DATA;
    }
    offset += sizeof(algo_id);

    cx_hash_sha256(data, offset, hash, sizeof(hash));

    if ((offset + sizeof(sig_length)) > lc) {
        return SWO_INCORRECT_DATA;
    }
    sig_length = data[offset];
    offset += sizeof(sig_length);

    if ((offset + sig_length) > lc) {
        return SWO_INCORRECT_DATA;
    }
    sig = &data[offset];
    offset += sig_length;

    // check for unexpected  extra data
    if (offset != lc) {
        return SWO_INCORRECT_DATA;
    }

    if (check_signature_with_pubkey(hash,
                                    sizeof(hash),
                                    LEDGER_NFT_METADATA_PUBLIC_KEY,
                                    sizeof(LEDGER_NFT_METADATA_PUBLIC_KEY),
                                    CERTIFICATE_PUBLIC_KEY_USAGE_NFT_METADATA,
                                    sig,
                                    sig_length) != true) {
        return SWO_INCORRECT_DATA;
    }

    if ((index = set_nft_info(&info)) == -1) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    if (index > UINT8_MAX) {
        // Could not represent it on one byte
        return SWO_INCORRECT_DATA;
    }

    G_io_tx_buffer[0] = (uint8_t) index;
    *tx += 1;
    return SWO_SUCCESS;
}
