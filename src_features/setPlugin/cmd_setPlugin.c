#include "shared_context.h"
#include "apdu_constants.h"
#include "asset_info.h"
#include "eth_plugin_interface.h"
#include "eth_plugin_internal.h"
#include "plugin_utils.h"
#include "common_utils.h"
#include "common_ui.h"
#include "os_io_seproxyhal.h"
#include "network.h"
#include "public_keys.h"
#ifdef HAVE_LEDGER_PKI
#include "os_pki.h"
#endif

// Supported internal plugins
#define ERC721_STR  "ERC721"
#define ERC1155_STR "ERC1155"

#define TYPE_SIZE               1
#define VERSION_SIZE            1
#define PLUGIN_NAME_LENGTH_SIZE 1
#define CHAIN_ID_SIZE           8
#define KEY_ID_SIZE             1
#define ALGORITHM_ID_SIZE       1
#define SIGNATURE_LENGTH_SIZE   1

#define HEADER_SIZE (TYPE_SIZE + VERSION_SIZE + PLUGIN_NAME_LENGTH_SIZE)

#define MIN_DER_SIG_SIZE 67
#define MAX_DER_SIG_SIZE 72

typedef enum Type {
    ETH_PLUGIN = 0x01,
} Type;

typedef enum Version {
    VERSION_1 = 0x01,
} Version;

typedef enum KeyId {
    TEST_PLUGIN_KEY = 0x00,
    // Must ONLY be used with ERC721 and ERC1155 plugin
    PROD_PLUGIN_KEY = 0x02,
} KeyId;

// Algorithm Id consists of a Key spec and an algorithm spec.
// Format is: KEYSPEC__ALGOSPEC
typedef enum AlgorithmID {
    ECC_SECG_P256K1__ECDSA_SHA_256 = 0x01,
} AlgorithmID;

// Verification function used to verify the signature
typedef bool verificationAlgo(const cx_ecfp_public_key_t *,
                              int,
                              cx_md_t,
                              const unsigned char *,
                              unsigned int,
                              unsigned char *,
                              unsigned int);

// Returns the plugin type of a given plugin name.
// If the plugin name is not a specific known internal plugin, this function default return value is
// `EXTERNAL`.
static pluginType_t getPluginType(char *pluginName, uint8_t pluginNameLength) {
    if (pluginNameLength == sizeof(ERC721_STR) - 1 &&
        strncmp(pluginName, ERC721_STR, pluginNameLength) == 0) {
        PRINTF("Using internal plugin ERC721\n");
        return ERC721;
    } else if (pluginNameLength == sizeof(ERC1155_STR) - 1 &&
               strncmp(pluginName, ERC1155_STR, pluginNameLength) == 0) {
        PRINTF("Using internal plugin ERC1155\n");
        return ERC1155;
    } else {
        PRINTF("Using external plugin\n");
        return EXTERNAL;
    }
}

void set_swap_with_calldata_plugin_type(void) {
    PRINTF("Using internal plugin SWAP_WITH_CALLDATA\n");
    pluginType = SWAP_WITH_CALLDATA;
}

uint16_t handleSetPlugin(const uint8_t *workBuffer, uint8_t dataLength) {
    PRINTF("Handling set Plugin\n");
    uint8_t hash[INT256_LENGTH] = {0};
    tokenContext_t *tokenContext = &dataContext.tokenContext;
    size_t offset = 0;
    uint8_t pluginNameLength = 0;
    size_t payloadSize = 0;
    uint64_t chain_id = 0;
    uint8_t signatureLen = 0;
    cx_err_t error = CX_INTERNAL_ERROR;
#ifdef HAVE_NFT_STAGING_KEY
    enum KeyId valid_keyId = TEST_PLUGIN_KEY;
#else
    enum KeyId valid_keyId = PROD_PLUGIN_KEY;
#endif
    enum KeyId keyId;
    uint32_t params[2];

    if (dataLength <= HEADER_SIZE) {
        PRINTF("Data too small for headers: expected at least %d, got %d\n",
               HEADER_SIZE,
               dataLength);
        return APDU_RESPONSE_INVALID_DATA;
    }

    if (workBuffer[offset] != ETH_PLUGIN) {
        PRINTF("Unsupported type %d\n", workBuffer[offset]);
        return APDU_RESPONSE_INVALID_DATA;
    }
    offset += TYPE_SIZE;

    if (workBuffer[offset] != VERSION_1) {
        PRINTF("Unsupported version %d\n", workBuffer[offset]);
        return APDU_RESPONSE_INVALID_DATA;
    }
    offset += VERSION_SIZE;

    pluginNameLength = workBuffer[offset];
    offset += PLUGIN_NAME_LENGTH_SIZE;

    // Size of the payload (everything except the signature)
    payloadSize = HEADER_SIZE + pluginNameLength + ADDRESS_LENGTH + SELECTOR_SIZE + CHAIN_ID_SIZE +
                  KEY_ID_SIZE + ALGORITHM_ID_SIZE;
    if (dataLength < payloadSize) {
        PRINTF("Data too small for payload: expected at least %d, got %d\n",
               payloadSize,
               dataLength);
        return APDU_RESPONSE_INVALID_DATA;
    }

    // `+ 1` because we want to add a null terminating character.
    if (pluginNameLength + 1 > sizeof(tokenContext->pluginName)) {
        PRINTF("plugin name too big: expected max %d, got %d\n",
               sizeof(dataContext.tokenContext.pluginName),
               pluginNameLength + 1);
        return APDU_RESPONSE_INVALID_DATA;
    }

    // Safe because we've checked the size before.
    memcpy(tokenContext->pluginName, workBuffer + offset, pluginNameLength);
    tokenContext->pluginName[pluginNameLength] = '\0';

    PRINTF("Length: %d\n", pluginNameLength);
    PRINTF("plugin name: %s\n", tokenContext->pluginName);
    offset += pluginNameLength;

    memcpy(tokenContext->contractAddress, workBuffer + offset, ADDRESS_LENGTH);
    PRINTF("Address: %.*H\n", ADDRESS_LENGTH, workBuffer + offset);
    offset += ADDRESS_LENGTH;

    memcpy(tokenContext->methodSelector, workBuffer + offset, SELECTOR_SIZE);
    PRINTF("Selector: %.*H\n", SELECTOR_SIZE, tokenContext->methodSelector);
    offset += SELECTOR_SIZE;

    chain_id = u64_from_BE(workBuffer + offset, CHAIN_ID_SIZE);
    // this prints raw data, so to have a more meaningful print, display
    // the buffer before the endianness swap
    PRINTF("ChainID: %.*H\n", sizeof(chain_id), (workBuffer + offset));
    if (!app_compatible_with_chain_id(&chain_id)) {
        UNSUPPORTED_CHAIN_ID_MSG(chain_id);
        return APDU_RESPONSE_INVALID_DATA;
    }
    offset += CHAIN_ID_SIZE;

    keyId = workBuffer[offset];
    if (keyId != valid_keyId) {
        PRINTF("Unsupported KeyID %d\n", keyId);
        return APDU_RESPONSE_INVALID_DATA;
    }
    offset += KEY_ID_SIZE;

    if (workBuffer[offset] != ECC_SECG_P256K1__ECDSA_SHA_256) {
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

    error = check_signature_with_pubkey("Set Plugin",
                                        hash,
                                        sizeof(hash),
                                        LEDGER_NFT_SELECTOR_PUBLIC_KEY,
                                        sizeof(LEDGER_NFT_SELECTOR_PUBLIC_KEY),
#ifdef HAVE_LEDGER_PKI
                                        CERTIFICATE_PUBLIC_KEY_USAGE_PLUGIN_METADATA,
#endif
                                        (uint8_t *) (workBuffer + offset),
                                        signatureLen);
    if (error != CX_OK) {
        PRINTF("Invalid signature\n");
#ifndef HAVE_BYPASS_SIGNATURES
        return APDU_RESPONSE_INVALID_DATA;
#endif
    }

    pluginType = getPluginType(tokenContext->pluginName, pluginNameLength);
    if (keyId == PROD_PLUGIN_KEY) {
        if (pluginType != ERC721 && pluginType != ERC1155) {
            PRINTF("AWS key must only be used to set NFT internal plugins\n");
            return APDU_RESPONSE_INVALID_DATA;
        }
    }

    if (pluginType == EXTERNAL) {
        PRINTF("Check external plugin %s\n", tokenContext->pluginName);

        // Check if the plugin is present on the device
        params[0] = (uint32_t) tokenContext->pluginName;
        params[1] = ETH_PLUGIN_CHECK_PRESENCE;
        BEGIN_TRY {
            TRY {
                os_lib_call(params);
            }
            CATCH_OTHER(e) {
                PRINTF("%s external plugin is not present\n", tokenContext->pluginName);
                memset(tokenContext->pluginName, 0, sizeof(tokenContext->pluginName));
                CLOSE_TRY;
                return APDU_RESPONSE_PLUGIN_NOT_INSTALLED;
            }
            FINALLY {
            }
        }
        END_TRY;
    }

    return APDU_RESPONSE_OK;
}
