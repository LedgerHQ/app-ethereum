#include "eth_plugin_internal.h"

bool erc20_plugin_available_check(void);
bool erc721_plugin_available_check(void);

void erc20_plugin_call(int message, void* parameters);
void erc721_plugin_call(int message, void* parameters);
void compound_plugin_call(int message, void* parameters);
void yearn_plugin_call(int message, void* parameters);
#ifdef HAVE_STARKWARE
void starkware_plugin_call(int message, void* parameters);
#endif
#ifdef HAVE_ETH2
void eth2_plugin_call(int message, void* parameters);
#endif

static const uint8_t const ERC20_TRANSFER_SELECTOR[SELECTOR_SIZE] = {0xa9, 0x05, 0x9c, 0xbb};
static const uint8_t const ERC20_APPROVE_SELECTOR[SELECTOR_SIZE] = {0x09, 0x5e, 0xa7, 0xb3};

const uint8_t* const ERC20_SELECTORS[NUM_ERC20_SELECTORS] = {ERC20_TRANSFER_SELECTOR,
                                                             ERC20_APPROVE_SELECTOR};

static const uint8_t const ERC721_APPROVE_SELECTOR[SELECTOR_SIZE] = {0x09, 0x5e, 0xa7, 0xb3};

const uint8_t* const ERC721_SELECTORS[NUM_ERC721_SELECTORS] = {ERC721_APPROVE_SELECTOR};

static const uint8_t const COMPOUND_REDEEM_UNDERLYING_SELECTOR[SELECTOR_SIZE] = {0x85,
                                                                                 0x2a,
                                                                                 0x12,
                                                                                 0xe3};
static const uint8_t const COMPOUND_REDEEM_SELECTOR[SELECTOR_SIZE] = {0xdb, 0x00, 0x6a, 0x75};
static const uint8_t const COMPOUND_MINT_SELECTOR[SELECTOR_SIZE] = {0xa0, 0x71, 0x2d, 0x68};
static const uint8_t const CETH_MINT_SELECTOR[SELECTOR_SIZE] = {0x12, 0x49, 0xc5, 0x8b};

const uint8_t* const COMPOUND_SELECTORS[NUM_COMPOUND_SELECTORS] = {
    COMPOUND_REDEEM_UNDERLYING_SELECTOR,
    COMPOUND_REDEEM_SELECTOR,
    COMPOUND_MINT_SELECTOR,
    CETH_MINT_SELECTOR};


static const uint8_t const YEARN_WITHDRAW_SELECTOR[SELECTOR_SIZE] = {0x3c, 0xcf, 0xd6, 0x0b};
static const uint8_t const YEARN_DEPOSIT_SELECTOR[SELECTOR_SIZE] = {0xd0, 0xe3, 0x0d, 0xb0};
static const uint8_t const YEARN_WITHDRAW_WITH_AMOUNT_SELECTOR[SELECTOR_SIZE] = {0x2e, 0x1a, 0x7d, 0x4d};
static const uint8_t const YEARN_DEPOSIT_WITH_AMOUNT_SELECTOR[SELECTOR_SIZE] = {0xb6, 0xb5, 0x5f, 0x25};

const uint8_t* const YEARN_SELECTORS[NUM_YEARN_SELECTORS] = {
    YEARN_WITHDRAW_SELECTOR,
    YEARN_DEPOSIT_SELECTOR,
    YEARN_WITHDRAW_WITH_AMOUNT_SELECTOR,
    YEARN_DEPOSIT_WITH_AMOUNT_SELECTOR};


#ifdef HAVE_ETH2

static const uint8_t const ETH2_DEPOSIT_SELECTOR[SELECTOR_SIZE] = {0x22, 0x89, 0x51, 0x18};

const uint8_t* const ETH2_SELECTORS[NUM_ETH2_SELECTORS] = {ETH2_DEPOSIT_SELECTOR};

#endif

#ifdef HAVE_STARKWARE

static const uint8_t const STARKWARE_REGISTER_ID[SELECTOR_SIZE] = {0xdd, 0x24, 0x14, 0xd4};
static const uint8_t const STARKWARE_DEPOSIT_TOKEN_ID[SELECTOR_SIZE] = {0x25, 0x05, 0xc3, 0xd9};
static const uint8_t const STARKWARE_DEPOSIT_ETH_ID[SELECTOR_SIZE] = {0x00, 0xae, 0xef, 0x8a};
static const uint8_t const STARKWARE_DEPOSIT_CANCEL_ID[SELECTOR_SIZE] = {0x7d, 0xf7, 0xdc, 0x04};
static const uint8_t const STARKWARE_DEPOSIT_RECLAIM_ID[SELECTOR_SIZE] = {0xae, 0x87, 0x38, 0x16};
static const uint8_t const STARKWARE_WITHDRAW_ID[SELECTOR_SIZE] = {0x44, 0x1a, 0x3e, 0x70};
static const uint8_t const STARKWARE_FULL_WITHDRAWAL_ID[SELECTOR_SIZE] = {0xa9, 0x33, 0x10, 0xc4};
static const uint8_t const STARKWARE_FREEZE_ID[SELECTOR_SIZE] = {0x93, 0xc1, 0xe4, 0x66};
static const uint8_t const STARKWARE_ESCAPE_ID[SELECTOR_SIZE] = {0x9e, 0x3a, 0xda, 0xc4};
static const uint8_t const STARKWARE_VERIFY_ESCAPE_ID[SELECTOR_SIZE] = {0x2d, 0xd5, 0x30, 0x06};

static const uint8_t const STARKWARE_WITHDRAW_TO_ID[SELECTOR_SIZE] = {0x14, 0xcd, 0x70, 0xe4};
static const uint8_t const STARKWARE_DEPOSIT_NFT_ID[SELECTOR_SIZE] = {0xae, 0x1c, 0xdd, 0xe6};
static const uint8_t const STARKWARE_DEPOSIT_NFT_RECLAIM_ID[SELECTOR_SIZE] = {0xfc,
                                                                              0xb0,
                                                                              0x58,
                                                                              0x22};
static const uint8_t const STARKWARE_WITHDRAW_AND_MINT_ID[SELECTOR_SIZE] = {0xd9, 0x14, 0x43, 0xb7};
static const uint8_t const STARKWARE_WITHDRAW_NFT_ID[SELECTOR_SIZE] = {0x01, 0x9b, 0x41, 0x7a};
static const uint8_t const STARKWARE_WITHDRAW_NFT_TO_ID[SELECTOR_SIZE] = {0xeb, 0xef, 0x0f, 0xd0};
static const uint8_t const STARKWARE_REGISTER_AND_DEPOSIT_TOKEN_ID[SELECTOR_SIZE] = {0x10,
                                                                                     0x82,
                                                                                     0x08,
                                                                                     0xcf};
static const uint8_t const STARKWARE_REGISTER_AND_DEPOSIT_ETH_ID[SELECTOR_SIZE] = {0xa7,
                                                                                   0x78,
                                                                                   0xc0,
                                                                                   0xc3};
static const uint8_t const STARKWARE_PROXY_DEPOSIT_TOKEN_ID[SELECTOR_SIZE] = {0xdc,
                                                                              0xca,
                                                                              0xd5,
                                                                              0x24};
static const uint8_t const STARKWARE_PROXY_DEPOSIT_ETH_ID[SELECTOR_SIZE] = {0x6c, 0xe5, 0xd9, 0x57};

const uint8_t* const STARKWARE_SELECTORS[NUM_STARKWARE_SELECTORS] = {
    STARKWARE_REGISTER_ID,
    STARKWARE_DEPOSIT_TOKEN_ID,
    STARKWARE_DEPOSIT_ETH_ID,
    STARKWARE_DEPOSIT_CANCEL_ID,
    STARKWARE_DEPOSIT_RECLAIM_ID,
    STARKWARE_WITHDRAW_ID,
    STARKWARE_FULL_WITHDRAWAL_ID,
    STARKWARE_FREEZE_ID,
    STARKWARE_ESCAPE_ID,
    STARKWARE_VERIFY_ESCAPE_ID,
    STARKWARE_WITHDRAW_TO_ID,
    STARKWARE_DEPOSIT_NFT_ID,
    STARKWARE_DEPOSIT_NFT_RECLAIM_ID,
    STARKWARE_WITHDRAW_AND_MINT_ID,
    STARKWARE_WITHDRAW_NFT_ID,
    STARKWARE_WITHDRAW_NFT_TO_ID,
    STARKWARE_REGISTER_AND_DEPOSIT_TOKEN_ID,
    STARKWARE_REGISTER_AND_DEPOSIT_ETH_ID,
    STARKWARE_PROXY_DEPOSIT_TOKEN_ID,
    STARKWARE_PROXY_DEPOSIT_ETH_ID};

#endif

// All internal alias names start with 'minus'

const internalEthPlugin_t const INTERNAL_ETH_PLUGINS[] = {
    {erc20_plugin_available_check,
     (const uint8_t**) ERC20_SELECTORS,
     NUM_ERC20_SELECTORS,
     "-erc20",
     erc20_plugin_call},

    {erc721_plugin_available_check,
     (const uint8_t**) ERC721_SELECTORS,
     NUM_ERC721_SELECTORS,
     "-er721",
     erc721_plugin_call},

    {NULL,
     (const uint8_t**) COMPOUND_SELECTORS,
     NUM_COMPOUND_SELECTORS,
     "-cmpd",
     compound_plugin_call},

    {NULL,
     (const uint8_t**) YEARN_SELECTORS,
     NUM_YEARN_SELECTORS,
     "-yearn",
     yearn_plugin_call},

#ifdef HAVE_ETH2

    {NULL, (const uint8_t**) ETH2_SELECTORS, NUM_ETH2_SELECTORS, "-eth2", eth2_plugin_call},

#endif

#ifdef HAVE_STARKWARE

    {NULL,
     (const uint8_t**) STARKWARE_SELECTORS,
     NUM_STARKWARE_SELECTORS,
     "-strk",
     starkware_plugin_call},

#endif

    {NULL, NULL, 0, "", NULL}};
