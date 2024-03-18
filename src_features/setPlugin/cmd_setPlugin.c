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
        return ERC721;
    } else if (pluginNameLength == sizeof(ERC1155_STR) - 1 &&
               strncmp(pluginName, ERC1155_STR, pluginNameLength) == 0) {
        return ERC1155;
    } else {
        return EXTERNAL;
    }
}

void handleSetPlugin(uint8_t p1,
                     uint8_t p2,
                     const uint8_t *workBuffer,
                     uint8_t dataLength,
                     unsigned int *flags,
                     unsigned int *tx) {
    UNUSED(p1);
    UNUSED(p2);
    UNUSED(flags);
    PRINTF("Handling set Plugin\n");
    uint8_t hash[INT256_LENGTH] = {0};
    cx_ecfp_public_key_t pluginKey = {0};
    tokenContext_t *tokenContext = &dataContext.tokenContext;

    size_t offset = 0;

    if (dataLength <= HEADER_SIZE) {
        PRINTF("Data too small for headers: expected at least %d, got %d\n",
               HEADER_SIZE,
               dataLength);
        THROW(0x6A80);
    }

    enum Type type = workBuffer[offset];
    PRINTF("Type: %d\n", type);
    switch (type) {
        case ETH_PLUGIN:
            break;
        default:
            PRINTF("Unsupported type %d\n", type);
            THROW(0x6a80);
            break;
    }
    offset += TYPE_SIZE;

    uint8_t version = workBuffer[offset];
    PRINTF("version: %d\n", version);
    switch (version) {
        case VERSION_1:
            break;
        default:
            PRINTF("Unsupported version %d\n", version);
            THROW(0x6a80);
            break;
    }
    offset += VERSION_SIZE;

    uint8_t pluginNameLength = workBuffer[offset];
    offset += PLUGIN_NAME_LENGTH_SIZE;

    // Size of the payload (everything except the signature)
    size_t payloadSize = HEADER_SIZE + pluginNameLength + ADDRESS_LENGTH + SELECTOR_SIZE +
                         CHAIN_ID_SIZE + KEY_ID_SIZE + ALGORITHM_ID_SIZE;
    if (dataLength < payloadSize) {
        PRINTF("Data too small for payload: expected at least %d, got %d\n",
               payloadSize,
               dataLength);
        THROW(0x6A80);
    }

    // `+ 1` because we want to add a null terminating character.
    if (pluginNameLength + 1 > sizeof(tokenContext->pluginName)) {
        PRINTF("plugin name too big: expected max %d, got %d\n",
               sizeof(dataContext.tokenContext.pluginName),
               pluginNameLength + 1);
        THROW(0x6A80);
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

    uint64_t chain_id = u64_from_BE(workBuffer + offset, CHAIN_ID_SIZE);
    // this prints raw data, so to have a more meaningful print, display
    // the buffer before the endianness swap
    PRINTF("ChainID: %.*H\n", sizeof(chain_id), (workBuffer + offset));
    if (!app_compatible_with_chain_id(&chain_id)) {
        UNSUPPORTED_CHAIN_ID_MSG(chain_id);
        THROW(APDU_RESPONSE_INVALID_DATA);
    }
    offset += CHAIN_ID_SIZE;

    enum KeyId keyId = workBuffer[offset];
    uint8_t const *rawKey;
    uint8_t rawKeyLen;

    PRINTF("KeyID: %d\n", keyId);
    switch (keyId) {
#ifdef HAVE_NFT_STAGING_KEY
        case TEST_PLUGIN_KEY:
#endif
        case PROD_PLUGIN_KEY:
            rawKey = LEDGER_NFT_SELECTOR_PUBLIC_KEY;
            rawKeyLen = sizeof(LEDGER_NFT_SELECTOR_PUBLIC_KEY);
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

    if (algorithmId != ECC_SECG_P256K1__ECDSA_SHA_256) {
        PRINTF("Incorrect algorithmId %d\n", algorithmId);
        THROW(0x6a80);
    }
    offset += ALGORITHM_ID_SIZE;
    PRINTF("hashing: %.*H\n", payloadSize, workBuffer);
    cx_hash_sha256(workBuffer, payloadSize, hash, sizeof(hash));

    if (dataLength < payloadSize + SIGNATURE_LENGTH_SIZE) {
        PRINTF("Data too short to hold signature length\n");
        THROW(0x6a80);
    }

    uint8_t signatureLen = workBuffer[offset];
    PRINTF("Signature len: %d\n", signatureLen);
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

    CX_ASSERT(cx_ecfp_init_public_key_no_throw(CX_CURVE_256K1, rawKey, rawKeyLen, &pluginKey));
    if (!cx_ecdsa_verify_no_throw(&pluginKey,
                                  hash,
                                  sizeof(hash),
                                  (unsigned char *) (workBuffer + offset),
                                  signatureLen)) {
#ifndef HAVE_BYPASS_SIGNATURES
        PRINTF("Invalid NFT signature\n");
        THROW(0x6A80);
#endif
    }

    pluginType = getPluginType(tokenContext->pluginName, pluginNameLength);
    if (keyId == PROD_PLUGIN_KEY) {
        if (pluginType != ERC721 && pluginType != ERC1155) {
            PRINTF("AWS key must only be used to set NFT internal plugins\n");
            THROW(0x6A80);
        }
    }

    switch (pluginType) {
        case EXTERNAL: {
            PRINTF("Check external plugin %s\n", tokenContext->pluginName);

            // Check if the plugin is present on the device
            uint32_t params[2];
            params[0] = (uint32_t) tokenContext->pluginName;
            params[1] = ETH_PLUGIN_CHECK_PRESENCE;
            BEGIN_TRY {
                TRY {
                    os_lib_call(params);
                }
                CATCH_OTHER(e) {
                    PRINTF("%s external plugin is not present\n", tokenContext->pluginName);
                    memset(tokenContext->pluginName, 0, sizeof(tokenContext->pluginName));
                    THROW(0x6984);
                }
                FINALLY {
                }
            }
            END_TRY;
            break;
        }
        default:
            break;
    }

    G_io_apdu_buffer[(*tx)++] = 0x90;
    G_io_apdu_buffer[(*tx)++] = 0x00;
}
