#ifndef _SHARED_CONTEXT_H_
#define _SHARED_CONTEXT_H_

#include <stdbool.h>
#include <stdint.h>

#include "os.h"
#include "cx.h"
#include "bip32.h"
#include "bip32_utils.h"
#include "ethUstream.h"
#include "tx_content.h"
#include "chainConfig.h"
#include "asset_info.h"
#ifdef HAVE_NBGL
#include "nbgl_types.h"
#endif

extern void app_exit(void);
extern void common_app_init(void);

#define SELECTOR_LENGTH 4

#define PLUGIN_ID_LENGTH 30

#define N_storage (*(volatile internalStorage_t *) PIC(&N_storage_real))

#define MAX_ASSETS 5

typedef struct internalStorage_t {
    bool dataAllowed;
    bool contractDetails;
    bool displayNonce;
#ifdef HAVE_EIP712_FULL_SUPPORT
    bool verbose_eip712;
#endif  // HAVE_EIP712_FULL_SUPPORT
#ifdef HAVE_TRUSTED_NAME
    bool verbose_trusted_name;
#endif  // HAVE_TRUSTED_NAME
    bool initialized;
} internalStorage_t;

typedef struct tokenContext_t {
    char pluginName[PLUGIN_ID_LENGTH];

    uint8_t data[INT256_LENGTH];
    uint16_t fieldIndex;
    uint8_t fieldOffset;

    uint8_t pluginUiMaxItems;
    uint8_t pluginUiCurrentItem;
    uint8_t pluginUiState;

    union {
        struct {
            uint8_t contractAddress[ADDRESS_LENGTH];
            uint8_t methodSelector[SELECTOR_LENGTH];
        };
        // This needs to be strictly 4 bytes aligned since pointers to it will be casted as
        // plugin context struct pointers (structs that contain up to 4 bytes wide elements)
        // uint8_t pluginContext[5 * INT256_LENGTH] __attribute__((aligned(4)));
        // TODO: use PLUGIN_CONTEXT_SIZE after eth is released with the updated plugin sdk
        uint8_t pluginContext[10 * INT256_LENGTH] __attribute__((aligned(4)));
    };

    uint8_t pluginStatus;

} tokenContext_t;

_Static_assert((offsetof(tokenContext_t, pluginContext) % 4) == 0, "Plugin context not aligned");

typedef struct publicKeyContext_t {
    cx_ecfp_public_key_t publicKey;
    char address[41];
    uint8_t chainCode[INT256_LENGTH];
    bool getChaincode;
} publicKeyContext_t;

typedef struct transactionContext_t {
    bip32_path_t bip32;
    uint8_t hash[INT256_LENGTH];
    union extraInfo_t extraInfo[MAX_ASSETS];
    bool assetSet[MAX_ASSETS];
    uint8_t currentAssetIndex;
} transactionContext_t;

typedef struct messageSigningContext_t {
    bip32_path_t bip32;
    uint8_t hash[INT256_LENGTH];
    uint32_t remainingLength;
} messageSigningContext_t;

typedef struct messageSigningContext712_t {
    bip32_path_t bip32;
    uint8_t domainHash[32];
    uint8_t messageHash[32];
} messageSigningContext712_t;

typedef union {
    publicKeyContext_t publicKeyContext;
    transactionContext_t transactionContext;
    messageSigningContext_t messageSigningContext;
    messageSigningContext712_t messageSigningContext712;
} tmpCtx_t;

typedef union {
    txContent_t txContent;
    cx_sha256_t sha2;
    char tmp[100];
} tmpContent_t;

typedef union {
    tokenContext_t tokenContext;
} dataContext_t;

typedef enum { APP_STATE_IDLE, APP_STATE_SIGNING_TX, APP_STATE_SIGNING_MESSAGE } app_state_t;

typedef enum {
    CONTRACT_NONE,
    CONTRACT_ERC20,
    CONTRACT_ALLOWANCE,
} contract_call_t;

// must be able to hold in decimal up to : floor(MAX_UINT64 / 2) - 36
#define NETWORK_STRING_MAX_SIZE 19

typedef struct txStringProperties_s {
    char fromAddress[43];
    char toAddress[43];
    char fullAmount[MAX_TICKER_LEN + 1 + 78 + 1];  // 2^256 is 78 digits long
    char maxFee[50];
    char nonce[8];  // 10M tx per account ought to be enough for everybody
    char network_name[NETWORK_STRING_MAX_SIZE + 1];
    char tx_hash[2 + (INT256_LENGTH * 2) + 1];
} txStringProperties_t;

#ifdef TARGET_NANOS
#define SHARED_CTX_FIELD_1_SIZE 100
#else
#ifdef SCREEN_SIZE_WALLET
#define SHARED_CTX_FIELD_1_SIZE 380
#else
#define SHARED_CTX_FIELD_1_SIZE 256
#endif
#endif
#define SHARED_CTX_FIELD_2_SIZE 40

typedef struct strDataTmp_s {
    char tmp[SHARED_CTX_FIELD_1_SIZE];
    char tmp2[SHARED_CTX_FIELD_2_SIZE];
} strDataTmp_t;

typedef union {
    txStringProperties_t common;
    strDataTmp_t tmp;
} strings_t;

extern const chain_config_t *chainConfig;

extern tmpCtx_t tmpCtx;
extern txContext_t txContext;
extern tmpContent_t tmpContent;
extern dataContext_t dataContext;
extern strings_t strings;
extern cx_sha3_t global_sha3;
extern const internalStorage_t N_storage_real;

typedef enum swap_mode_e {
    SWAP_MODE_STANDARD,
    SWAP_MODE_CROSSCHAIN_PENDING_CHECK,
    SWAP_MODE_CROSSCHAIN_SUCCESS,
    SWAP_MODE_ERROR,
} swap_mode_t;

extern bool G_called_from_swap;
extern bool G_swap_response_ready;
extern swap_mode_t G_swap_mode;
extern uint8_t G_swap_crosschain_hash[CX_SHA256_SIZE];

typedef enum {
    // External plugin, set by setExternalPlugin
    EXTERNAL,
    // Specific SWAP_WITH_CALLDATA internal plugin
    // set as fallback when started if calldata is provided in swap mode
    SWAP_WITH_CALLDATA,
    // Specific ERC721 internal plugin, set by setPlugin
    ERC721,
    // Specific ERC1155 internal plugin, set by setPlugin
    ERC1155,
    // Old internal plugin, not set by any command
    OLD_INTERNAL
} pluginType_t;

extern pluginType_t pluginType;

extern uint8_t appState;
#ifdef HAVE_ETH2
extern uint32_t eth2WithdrawalIndex;
#endif

void reset_app_context(void);
const uint8_t *parseBip32(const uint8_t *dataBuffer, uint8_t *dataLength, bip32_path_t *bip32);
void storage_init(void);

#endif  // _SHARED_CONTEXT_H_
