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

#define N_storage (*(volatile internalStorage_t*) PIC(&N_storage_real))

typedef struct internalStorage_t {
  unsigned char dataAllowed;
  unsigned char contractDetails;
  uint8_t initialized;
} internalStorage_t;

typedef struct tokenContext_t {
    char pluginName[PLUGIN_ID_LENGTH];
    uint8_t pluginAvailable;    

    uint8_t data[32];
    uint8_t fieldIndex;
    uint8_t fieldOffset;    

    uint8_t pluginUiMaxItems;
    uint8_t pluginUiCurrentItem;
    uint8_t pluginUiState;

    uint8_t pluginContext[3 * 32];

#ifdef HAVE_STARKWARE
    uint8_t quantum[32];
    uint8_t quantumIndex;
#endif

} tokenContext_t;

typedef struct publicKeyContext_t {
    cx_ecfp_public_key_t publicKey;
    uint8_t address[41];
    uint8_t chainCode[32];
    bool getChaincode;
} publicKeyContext_t;

typedef struct transactionContext_t {
    uint8_t pathLength;
    uint32_t bip32Path[MAX_BIP32_PATH];
    uint8_t hash[32];
    tokenDefinition_t tokens[MAX_TOKEN];
    uint8_t tokenSet[MAX_TOKEN];
    uint8_t currentTokenIndex;
} transactionContext_t;

typedef struct messageSigningContext_t {
    uint8_t pathLength;
    uint32_t bip32Path[MAX_BIP32_PATH];
    uint8_t hash[32];
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
} starkContext_t;

#endif

typedef union {
    tokenContext_t tokenContext;
#ifdef HAVE_STARKWARE
    starkContext_t starkContext;
#endif
} dataContext_t;

typedef enum {
  APP_STATE_IDLE,
  APP_STATE_SIGNING_TX,
  APP_STATE_SIGNING_MESSAGE
} app_state_t;

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

typedef struct txStringProperties_t {
    char fullAddress[43];
    char fullAmount[50];
    char maxFee[50];
} txStringProperties_t;

typedef struct strDataTmp_t {
    char tmp[100];
    char tmp2[40];
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

#ifdef TARGET_BLUE
extern bagl_element_t tmp_element;
extern char addressSummary[32];
#endif

extern bool called_from_swap;
extern uint8_t dataAllowed;
extern uint8_t contractDetails;
extern bool dataPresent;
extern uint8_t appState;
#ifdef HAVE_STARKWARE
extern bool quantumSet;
#endif

void reset_app_context(void);

#endif // __SHARED_CONTEXT_H__

