#include <string.h>
#include "eth_plugin_internal.h"
#include "plugin_utils.h"

void erc20_plugin_call(int message, void* parameters);

#ifdef HAVE_ETH2
void eth2_plugin_call(int message, void* parameters);
#endif

static const uint8_t ERC20_TRANSFER_SELECTOR[SELECTOR_SIZE] = {0xa9, 0x05, 0x9c, 0xbb};
static const uint8_t ERC20_APPROVE_SELECTOR[SELECTOR_SIZE] = {0x09, 0x5e, 0xa7, 0xb3};

const uint8_t* const ERC20_SELECTORS[NUM_ERC20_SELECTORS] = {ERC20_TRANSFER_SELECTOR,
                                                             ERC20_APPROVE_SELECTOR};

#ifdef HAVE_ETH2

static const uint8_t ETH2_DEPOSIT_SELECTOR[SELECTOR_SIZE] = {0x22, 0x89, 0x51, 0x18};

const uint8_t* const ETH2_SELECTORS[NUM_ETH2_SELECTORS] = {ETH2_DEPOSIT_SELECTOR};

#endif

// All internal alias names start with 'minus'

const internalEthPlugin_t INTERNAL_ETH_PLUGINS[] = {
    {NULL, ERC20_SELECTORS, NUM_ERC20_SELECTORS, "-erc20", erc20_plugin_call},

#ifdef HAVE_ETH2

    {NULL, ETH2_SELECTORS, NUM_ETH2_SELECTORS, "-eth2", eth2_plugin_call},

#endif

    {NULL, NULL, 0, "", NULL}};
