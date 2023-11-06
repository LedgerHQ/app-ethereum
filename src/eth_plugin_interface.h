#ifndef _ETH_PLUGIN_INTERFACE_H_
#define _ETH_PLUGIN_INTERFACE_H_

#include "os.h"
#include "cx.h"
#include "ethUstream.h"
#include "tokens.h"
#include "shared_context.h"

// Interface version. To be updated everytime we introduce breaking changes to the plugin interface.
typedef enum eth_plugin_interface_version_e {
    ETH_PLUGIN_INTERFACE_VERSION_1 = 1,
    ETH_PLUGIN_INTERFACE_VERSION_2 = 2,
    ETH_PLUGIN_INTERFACE_VERSION_3 = 3,
    ETH_PLUGIN_INTERFACE_VERSION_4 = 4,
    ETH_PLUGIN_INTERFACE_VERSION_5 = 5,
    ETH_PLUGIN_INTERFACE_VERSION_LATEST = 6,
} eth_plugin_interface_version_t;

typedef enum eth_plugin_msg_e {

    ETH_PLUGIN_INIT_CONTRACT = 0x0101,
    ETH_PLUGIN_PROVIDE_PARAMETER = 0x0102,
    ETH_PLUGIN_FINALIZE = 0x0103,
    ETH_PLUGIN_PROVIDE_INFO = 0x0104,
    ETH_PLUGIN_QUERY_CONTRACT_ID = 0x0105,
    ETH_PLUGIN_QUERY_CONTRACT_UI = 0x0106,
    ETH_PLUGIN_CHECK_PRESENCE = 0x01FF,

} eth_plugin_msg_t;

typedef enum eth_plugin_result_e {
    // Unsuccesful return values
    ETH_PLUGIN_RESULT_ERROR = 0x00,
    ETH_PLUGIN_RESULT_UNAVAILABLE = 0x01,
    ETH_PLUGIN_RESULT_UNSUCCESSFUL = 0x02,  // Used for comparison

    // Successful return values
    ETH_PLUGIN_RESULT_SUCCESSFUL = 0x03,  // Used for comparison
    ETH_PLUGIN_RESULT_OK = 0x04,
    ETH_PLUGIN_RESULT_OK_ALIAS = 0x05,
    ETH_PLUGIN_RESULT_FALLBACK = 0x06,

} eth_plugin_result_t;

typedef enum eth_ui_type_e {

    ETH_UI_TYPE_AMOUNT_ADDRESS = 0x01,
    ETH_UI_TYPE_GENERIC = 0x02,

} eth_ui_type_t;

typedef void (*PluginCall)(int, void *);

// Shared objects, read-write

typedef struct ethPluginSharedRW_s {
    cx_sha3_t *sha3;

} ethPluginSharedRW_t;

// Shared objects, read-only

typedef struct ethPluginSharedRO_s {
    txContent_t *txContent;

} ethPluginSharedRO_t;

// Init Contract

typedef struct ethPluginInitContract_s {
    uint8_t interfaceVersion;
    uint8_t result;

    // in
    ethPluginSharedRW_t *pluginSharedRW;
    ethPluginSharedRO_t *pluginSharedRO;
    uint8_t *pluginContext;
    size_t pluginContextLength;
    const uint8_t *selector;  // 4 bytes selector
    size_t dataSize;

    char *alias;  // 29 bytes alias if ETH_PLUGIN_RESULT_OK_ALIAS set

} ethPluginInitContract_t;

// Provide parameter

typedef struct ethPluginProvideParameter_s {
    ethPluginSharedRW_t *pluginSharedRW;
    ethPluginSharedRO_t *pluginSharedRO;
    uint8_t *pluginContext;
    const uint8_t *parameter;  // 32 bytes parameter
    uint32_t parameterOffset;

    uint8_t result;

} ethPluginProvideParameter_t;

// Finalize

typedef struct ethPluginFinalize_s {
    ethPluginSharedRW_t *pluginSharedRW;
    ethPluginSharedRO_t *pluginSharedRO;
    uint8_t *pluginContext;

    uint8_t *tokenLookup1;  // set by the plugin if a token should be looked up
    uint8_t *tokenLookup2;

    const uint8_t *amount;   // set an uint256 pointer if uiType is UI_AMOUNT_ADDRESS
    const uint8_t *address;  // set to the destination address if uiType is UI_AMOUNT_ADDRESS. Set
                             // to the user's address if uiType is UI_TYPE_GENERIC

    uint8_t uiType;
    uint8_t numScreens;  // ignored if uiType is UI_AMOUNT_ADDRESS
    uint8_t result;

} ethPluginFinalize_t;

// If uiType is UI_AMOUNT_ADDRESS, the amount and address provided by the plugin will be used
// If tokenLookup1 is set, the amount is provided for this token

// if uiType is UI_TYPE_GENERIC, the ETH application provides tokens if requested then prompts
// for each UI field
// The first field is forced by the ETH app to be the name + version of the plugin handling the
// request The last field is the fee amount

// Provide token

typedef struct ethPluginProvideInfo_s {
    ethPluginSharedRW_t *pluginSharedRW;
    ethPluginSharedRO_t *pluginSharedRO;
    uint8_t *pluginContext;

    union extraInfo_t *item1;  // set by the ETH application, to be saved by the plugin
    union extraInfo_t *item2;

    uint8_t additionalScreens;  // Used by the plugin if it needs to display additional screens
                                // based on the information received from the token definitions.

    uint8_t result;

} ethPluginProvideInfo_t;

// Query Contract name and version

// This is always called on the non aliased contract

typedef struct ethQueryContractID_s {
    ethPluginSharedRW_t *pluginSharedRW;
    ethPluginSharedRO_t *pluginSharedRO;
    uint8_t *pluginContext;

    char *name;
    size_t nameLength;
    char *version;
    size_t versionLength;

    uint8_t result;

} ethQueryContractID_t;

// Query Contract UI

typedef struct ethQueryContractUI_s {
    ethPluginSharedRW_t *pluginSharedRW;
    ethPluginSharedRO_t *pluginSharedRO;
    union extraInfo_t *item1;
    union extraInfo_t *item2;
    char network_ticker[MAX_TICKER_LEN];
    uint8_t *pluginContext;
    uint8_t screenIndex;

    char *title;
    size_t titleLength;
    char *msg;
    size_t msgLength;

    uint8_t result;

} ethQueryContractUI_t;

// Helper to check that the plugin context structure is not bigger than 5 * 32
#define PLUGIN_CONTEXT_SIZE (5 * INT256_LENGTH)
#define ASSERT_SIZEOF_PLUGIN_CONTEXT(s) \
    _Static_assert(sizeof(s) <= PLUGIN_CONTEXT_SIZE, "Plugin context structure is too big.")

#endif  // _ETH_PLUGIN_INTERFACE_H_
