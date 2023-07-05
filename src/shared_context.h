#ifndef _SHARED_CONTEXT_H_
#define _SHARED_CONTEXT_H_

#include <stdbool.h>
#include <stdint.h>
#include "ethUstream.h"
#include "tokens.h"
#include "chainConfig.h"
#include "nft.h"
#ifdef HAVE_NBGL
#include "nbgl_types.h"
#endif

#define MAX_BIP32_PATH 10

#define WEI_TO_ETHER 18

#define SELECTOR_LENGTH 4

#define PLUGIN_ID_LENGTH 30

#define N_storage (*(volatile internalStorage_t *) PIC(&N_storage_real))

typedef struct bip32_path_t {
    uint8_t length;
    uint32_t path[MAX_BIP32_PATH];
} bip32_path_t;

typedef struct internalStorage_t {
    bool dataAllowed;
    bool contractDetails;
    bool displayNonce;
#ifdef HAVE_EIP712_FULL_SUPPORT
    bool verbose_eip712;
#endif  // HAVE_EIP712_FULL_SUPPORT
#ifdef HAVE_DOMAIN_NAME
    bool verbose_domain_name;
#endif  // HAVE_DOMAIN_NAME
    bool initialized;
} internalStorage_t;

#ifdef HAVE_STARKWARE

typedef enum starkQuantumType_e {

    STARK_QUANTUM_LEGACY = 0x00,
    STARK_QUANTUM_ETH,
    STARK_QUANTUM_ERC20,
    STARK_QUANTUM_ERC721,
    STARK_QUANTUM_MINTABLE_ERC20,
    STARK_QUANTUM_MINTABLE_ERC721

} starkQuantumType_e;

#endif

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
        uint8_t pluginContext[5 * INT256_LENGTH] __attribute__((aligned(4)));
    };

    uint8_t pluginStatus;

#ifdef HAVE_STARKWARE
    uint8_t quantum[32];
    uint8_t mintingBlob[32];
    uint8_t quantumIndex;
    uint8_t quantumType;
#endif

} tokenContext_t;

_Static_assert((offsetof(tokenContext_t, pluginContext) % 4) == 0, "Plugin context not aligned");

typedef struct publicKeyContext_t {
    cx_ecfp_public_key_t publicKey;
    char address[41];
    uint8_t chainCode[INT256_LENGTH];
    bool getChaincode;
} publicKeyContext_t;

typedef union extraInfo_t {
    tokenDefinition_t token;
    nftInfo_t nft;
} extraInfo_t;

typedef struct transactionContext_t {
    bip32_path_t bip32;
    uint8_t hash[INT256_LENGTH];
    union extraInfo_t extraInfo[MAX_ITEMS];
    uint8_t tokenSet[MAX_ITEMS];
    uint8_t currentItemIndex;
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

#ifdef HAVE_STARKWARE

typedef struct starkContext_t {
    uint8_t w1[32];
    uint8_t w2[32];
    uint8_t w3[32];
    uint8_t w4[32];
    uint8_t conditional;
    uint8_t transferDestination[32];
    uint8_t fact[32];
    uint8_t conditionAddress[20];
} starkContext_t;

#endif

typedef union {
    tokenContext_t tokenContext;

#ifdef HAVE_STARKWARE
    starkContext_t starkContext;
#endif
} dataContext_t;

typedef enum { APP_STATE_IDLE, APP_STATE_SIGNING_TX, APP_STATE_SIGNING_MESSAGE } app_state_t;

typedef enum {
    CONTRACT_NONE,
    CONTRACT_ERC20,
    CONTRACT_ALLOWANCE,
#ifdef HAVE_STARKWARE
    CONTRACT_STARKWARE_REGISTER,
    CONTRACT_STARKWARE_DEPOSIT_TOKEN,
    CONTRACT_STARKWARE_DEPOSIT_ETH,
    CONTRACT_STARKWARE_WITHDRAW,
    CONTRACT_STARKWARE_DEPOSIT_CANCEL,
    CONTRACT_STARKWARE_DEPOSIT_RECLAIM,
    CONTRACT_STARKWARE_FULL_WITHDRAWAL,
    CONTRACT_STARKWARE_FREEZE,
    CONTRACT_STARKWARE_ESCAPE,
    CONTRACT_STARKWARE_VERIFY_ESCAPE
#endif
} contract_call_t;

#define NETWORK_STRING_MAX_SIZE 16

typedef struct txStringProperties_s {
    char fullAddress[43];
    char fullAmount[79];  // 2^256 is 78 digits long
    char maxFee[50];
    char nonce[8];  // 10M tx per account ought to be enough for everybody
    char network_name[NETWORK_STRING_MAX_SIZE];
} txStringProperties_t;

#ifdef TARGET_NANOS
#define SHARED_CTX_FIELD_1_SIZE 100
#else
#define SHARED_CTX_FIELD_1_SIZE 256
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

extern chain_config_t *chainConfig;

extern tmpCtx_t tmpCtx;
extern txContext_t txContext;
extern tmpContent_t tmpContent;
extern dataContext_t dataContext;
extern strings_t strings;
extern cx_sha3_t global_sha3;
extern const internalStorage_t N_storage_real;

extern bool G_called_from_swap;

typedef enum {
    EXTERNAL,     //  External plugin, set by setExternalPlugin.
    ERC721,       // Specific ERC721 internal plugin, set by setPlugin.
    ERC1155,      // Specific ERC1155 internal plugin, set by setPlugin
    OLD_INTERNAL  // Old internal plugin, not set by any command.
} pluginType_t;

extern pluginType_t pluginType;

typedef enum { CALLER_TYPE_CLONE, CALLER_TYPE_PLUGIN } e_caller_type;

typedef struct caller_app_t {
    const char *name;
#ifdef HAVE_NBGL
    const nbgl_icon_details_t *icon;
#endif
    char type;  // does not have to be set by the caller app
} caller_app_t;

extern uint8_t appState;
#ifdef HAVE_STARKWARE
extern bool quantumSet;
#endif
#ifdef HAVE_ETH2
extern uint32_t eth2WithdrawalIndex;
#endif

extern caller_app_t *caller_app;

void reset_app_context(void);
const uint8_t *parseBip32(const uint8_t *dataBuffer, uint8_t *dataLength, bip32_path_t *bip32);

#endif  // _SHARED_CONTEXT_H_
