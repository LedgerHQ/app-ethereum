#ifndef __SHARED_CONTEXT_H__

#define __SHARED_CONTEXT_H__

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "os.h"
#include "cx.h"
#include "os_io_seproxyhal.h"
#include "ethUstream.h"
#include "ethUtils.h"
#include "uint256.h"
#include "tokens.h"
#include "chainConfig.h"
#include "eth_plugin_interface.h"

#define MAX_BIP32_PATH 10

#define MAX_TOKEN 2

#define WEI_TO_ETHER 18

#define SELECTOR_LENGTH 4

#define N_storage (*(volatile internalStorage_t *) PIC(&N_storage_real))

typedef struct internalStorage_t {
    unsigned char dataAllowed;
    unsigned char contractDetails;
    unsigned char displayNonce;
    uint8_t initialized;
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
    uint8_t pluginStatus;

    uint8_t data[INT256_LENGTH];
    uint8_t fieldIndex;
    uint8_t fieldOffset;

    uint8_t pluginUiMaxItems;
    uint8_t pluginUiCurrentItem;
    uint8_t pluginUiState;

    union {
        struct {
            uint8_t contract_address[ADDRESS_LENGTH];
            uint8_t method_selector[SELECTOR_LENGTH];
        };
        uint8_t pluginContext[5 * INT256_LENGTH];
    };

#ifdef HAVE_STARKWARE
    uint8_t quantum[32];
    uint8_t mintingBlob[32];
    uint8_t quantumIndex;
    uint8_t quantumType;
#endif

} tokenContext_t;

typedef struct publicKeyContext_t {
    cx_ecfp_public_key_t publicKey;
    char address[41];
    uint8_t chainCode[INT256_LENGTH];
    bool getChaincode;
} publicKeyContext_t;

typedef struct transactionContext_t {
    uint8_t pathLength;
    uint32_t bip32Path[MAX_BIP32_PATH];
    uint8_t hash[INT256_LENGTH];
    tokenDefinition_t tokens[MAX_TOKEN];
    uint8_t tokenSet[MAX_TOKEN];
    uint8_t currentTokenIndex;
} transactionContext_t;

typedef struct messageSigningContext_t {
    uint8_t pathLength;
    uint32_t bip32Path[MAX_BIP32_PATH];
    uint8_t hash[INT256_LENGTH];
    uint32_t remainingLength;
} messageSigningContext_t;

typedef struct messageSigningContext712_t {
    uint8_t pathLength;
    uint32_t bip32Path[MAX_BIP32_PATH];
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

#define NETWORK_NAME_MAX_SIZE 12

typedef struct txStringProperties_t {
    char fullAddress[43];
    char fullAmount[50];
    char maxFee[50];
    char nonce[8];  // 10M tx per account ought to be enough for everybody
    char network_name[NETWORK_NAME_MAX_SIZE];
} txStringProperties_t;

#define SHARED_CTX_FIELD_1_SIZE 100
#define SHARED_CTX_FIELD_2_SIZE 40

typedef struct strDataTmp_t {
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

extern bool called_from_swap;
extern bool dataPresent;
extern bool externalPluginIsSet;
extern uint8_t appState;
#ifdef HAVE_STARKWARE
extern bool quantumSet;
#endif
#ifdef HAVE_ETH2
extern uint32_t eth2WithdrawalIndex;
#endif

void reset_app_context(void);

#endif  // __SHARED_CONTEXT_H__
