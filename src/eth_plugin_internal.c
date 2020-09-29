#include "eth_plugin_internal.h"

void erc20_plugin_call(int message, void *parameters);
void compound_plugin_call(int message, void *parameters);
void starkware_plugin_call(int message, void *parameters);

static const uint8_t const ERC20_TRANSFER_SELECTOR[SELECTOR_SIZE] = { 0xa9, 0x05, 0x9c, 0xbb };
static const uint8_t const ERC20_APPROVE_SELECTOR[SELECTOR_SIZE] = { 0x09, 0x5e, 0xa7, 0xb3 };

const uint8_t* const ERC20_SELECTORS[NUM_ERC20_SELECTORS] = {
	ERC20_TRANSFER_SELECTOR, ERC20_APPROVE_SELECTOR
};

static const uint8_t const COMPOUND_REDEEM_UNDERLYING_SELECTOR[SELECTOR_SIZE] = { 0x85, 0x2a, 0x12, 0xe3 };
static const uint8_t const COMPOUND_REDEEM_SELECTOR[SELECTOR_SIZE] = { 0xdb, 0x00, 0x6a, 0x75 };
static const uint8_t const COMPOUND_MINT_SELECTOR[SELECTOR_SIZE] = { 0xa0, 0x71, 0x2d, 0x68 };
static const uint8_t const CETH_MINT_SELECTOR[SELECTOR_SIZE] = { 0x12, 0x49, 0xc5, 0x8b };

const uint8_t* const COMPOUND_SELECTORS[NUM_COMPOUND_SELECTORS] = {
	COMPOUND_REDEEM_UNDERLYING_SELECTOR, COMPOUND_REDEEM_SELECTOR,
	COMPOUND_MINT_SELECTOR, CETH_MINT_SELECTOR
};

#ifdef HAVE_STARKWARE

static const uint8_t const STARKWARE_REGISTER_ID[SELECTOR_SIZE] = { 0x76, 0x57, 0x18, 0xd7 };
static const uint8_t const STARKWARE_DEPOSIT_TOKEN_ID[SELECTOR_SIZE] = { 0x00, 0xae, 0xef, 0x8a };
static const uint8_t const STARKWARE_DEPOSIT_ETH_ID[SELECTOR_SIZE] = { 0xe2, 0xbb, 0xb1, 0x58 };
static const uint8_t const STARKWARE_DEPOSIT_CANCEL_ID[SELECTOR_SIZE] = { 0xc7, 0xfb, 0x11, 0x7c };
static const uint8_t const STARKWARE_DEPOSIT_RECLAIM_ID[SELECTOR_SIZE] = { 0x4e, 0xab, 0x38, 0xf4 };
static const uint8_t const STARKWARE_WITHDRAW_ID[SELECTOR_SIZE] = { 0x2e, 0x1a, 0x7d, 0x4d };
static const uint8_t const STARKWARE_FULL_WITHDRAWAL_ID[SELECTOR_SIZE] = { 0x27, 0x6d, 0xd1, 0xde };
static const uint8_t const STARKWARE_FREEZE_ID[SELECTOR_SIZE] = { 0xb9, 0x10, 0x72, 0x09 };
static const uint8_t const STARKWARE_ESCAPE_ID[SELECTOR_SIZE] = { 0x9e, 0x3a, 0xda, 0xc4 };
static const uint8_t const STARKWARE_VERIFY_ESCAPE_ID[SELECTOR_SIZE] = { 0x2d, 0xd5, 0x30, 0x06 };

const uint8_t* const STARKWARE_SELECTORS[NUM_STARKWARE_SELECTORS] = {
	STARKWARE_REGISTER_ID, STARKWARE_DEPOSIT_TOKEN_ID, STARKWARE_DEPOSIT_ETH_ID,
	STARKWARE_DEPOSIT_CANCEL_ID, STARKWARE_DEPOSIT_RECLAIM_ID, STARKWARE_WITHDRAW_ID,
	STARKWARE_FULL_WITHDRAWAL_ID, STARKWARE_FREEZE_ID, STARKWARE_ESCAPE_ID,
	STARKWARE_VERIFY_ESCAPE_ID
};

#endif

// All internal alias names start with 'minus'

const internalEthPlugin_t const INTERNAL_ETH_PLUGINS[NUM_INTERNAL_PLUGINS] =  {
	{
		ERC20_SELECTORS,
		2,
		"-erc20",
		erc20_plugin_call
	},

	{
		COMPOUND_SELECTORS,
		4,
		"-cmpd",
		compound_plugin_call
	},

#ifdef HAVE_STARKWARE

	{
		STARKWARE_SELECTORS,
		10,
		"-strk",
		starkware_plugin_call
	},

#endif	
};
