#include "shared_context.h"
#include "apdu_constants.h"
#include "ui_flow.h"
#include "tokens.h"
#include "eth_plugin_interface.h"
#include "eth_plugin_internal.h"
#include "utils.h"

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

// Only used for signing NFT plugins (ERC721 and ERC1155)
static const uint8_t LEDGER_NFT_SELECTOR_PUBLIC_KEY[] = {
#ifdef HAVE_NFT_TESTING_KEY
    0x04, 0xf5, 0x70, 0x0c, 0xa1, 0xe8, 0x74, 0x24, 0xc7, 0xc7, 0xd1, 0x19, 0xe7, 0xe3,
    0xc1, 0x89, 0xb1, 0x62, 0x50, 0x94, 0xdb, 0x6e, 0xa0, 0x40, 0x87, 0xc8, 0x30, 0x00,
    0x7d, 0x0b, 0x46, 0x9a, 0x53, 0x11, 0xee, 0x6a, 0x1a, 0xcd, 0x1d, 0xa5, 0xaa, 0xb0,
    0xf5, 0xc6, 0xdf, 0x13, 0x15, 0x8d, 0x28, 0xcc, 0x12, 0xd1, 0xdd, 0xa6, 0xec, 0xe9,
    0x46, 0xb8, 0x9d, 0x5c, 0x05, 0x49, 0x92, 0x59, 0xc4
#else
    0x04, 0xd8, 0x62, 0x6e, 0x01, 0x9e, 0x55, 0x3e, 0x19, 0x69, 0x56, 0xf1, 0x17, 0x4d,
    0xcd, 0xb8, 0x9a, 0x1c, 0xda, 0xc4, 0x93, 0x90, 0x08, 0xbc, 0x79, 0x77, 0x33, 0x6d,
    0x78, 0x24, 0xee, 0xe3, 0xa2, 0x62, 0x24, 0x1a, 0x62, 0x73, 0x52, 0x3b, 0x09, 0xb8,
    0xd0, 0xce, 0x0d, 0x39, 0xe8, 0x60, 0xc9, 0x4d, 0x02, 0x53, 0x58, 0xdb, 0xdc, 0x25,
    0x92, 0xc7, 0xc6, 0x48, 0x0d, 0x39, 0xce, 0xbb, 0xa3
#endif
};

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
// `EXERNAL`.
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
                     uint8_t *workBuffer,
                     uint16_t dataLength,
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

    uint64_t chainId = u64_from_BE(workBuffer + offset, CHAIN_ID_SIZE);
    // this prints raw data, so to have a more meaningful print, display
    // the buffer before the endianness swap
    PRINTF("ChainID: %.*H\n", sizeof(chainId), (workBuffer + offset));
    if ((chainConfig->chainId != 0) && (chainConfig->chainId != chainId)) {
        PRINTF("Chain ID token mismatch\n");
        THROW(0x6A80);
    }
    offset += CHAIN_ID_SIZE;

    enum KeyId keyId = workBuffer[offset];
    uint8_t const *rawKey;
    uint8_t rawKeyLen;

    PRINTF("KeyID: %d\n", keyId);
    switch (keyId) {
#ifdef HAVE_NFT_TESTING_KEY
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
    cx_curve_t curve;
    verificationAlgo *verificationFn;
    cx_md_t hashId;

    switch (algorithmId) {
        case ECC_SECG_P256K1__ECDSA_SHA_256:
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

    cx_ecfp_init_public_key(curve, rawKey, rawKeyLen, &pluginKey);
    if (!verificationFn(&pluginKey,
                        CX_LAST,
                        hashId,
                        hash,
                        sizeof(hash),
                        workBuffer + offset,
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
