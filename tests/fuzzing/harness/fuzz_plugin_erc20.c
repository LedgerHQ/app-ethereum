/**
 * Fuzzing harness for the ERC20 internal plugin (token transfer/approve)
 *
 * The first byte of fuzz data selects which ERC20 selector to test:
 * - 0: TRANSFER (0xa9059cbb)
 * - 1: APPROVE (0x095ea7b3)
 */

#include "fuzz_utils.h"
#include "erc20_plugin.h"

// Buffer sizes for UI queries
#define NAME_LENGTH    32
#define VERSION_LENGTH 32
#define TITLE_LENGTH   32
#define MSG_LENGTH     79

// ERC20 selectors
static const uint8_t ERC20_TRANSFER_SELECTOR[SELECTOR_SIZE] = {0xa9, 0x05, 0x9c, 0xbb};
static const uint8_t ERC20_APPROVE_SELECTOR[SELECTOR_SIZE] = {0x09, 0x5e, 0xa7, 0xb3};

#define NUM_ERC20_SELECTORS_FUZZ 2
static const uint8_t *const ERC20_SELECTORS_FUZZ[NUM_ERC20_SELECTORS_FUZZ] = {
    ERC20_TRANSFER_SELECTOR,
    ERC20_APPROVE_SELECTOR,
};

static int fuzz_erc20_plugin(const uint8_t *data, size_t size) {
    ethPluginInitContract_t init_contract = {0};
    ethPluginProvideParameter_t provide_param = {0};
    ethPluginFinalize_t finalize = {0};
    ethPluginProvideInfo_t provide_info = {0};
    ethQueryContractID_t query_id = {0};
    ethQueryContractUI_t query_ui = {0};
    txContent_t content = {0};

    uint8_t plugin_context[PLUGIN_CONTEXT_SIZE] = {0};

    const uint8_t address[ADDRESS_LENGTH] = {0};

    char name[NAME_LENGTH] = {0};
    char version[VERSION_LENGTH] = {0};
    char title[TITLE_LENGTH] = {0};
    char msg[MSG_LENGTH] = {0};

    extraInfo_t item1 = {0};
    extraInfo_t item2 = {0};

    // Need at least 1 byte for selector choice
    if (size < 1) {
        return 0;
    }

    // Use first byte to select which ERC20 selector to test
    uint8_t selector_idx = data[0] % NUM_ERC20_SELECTORS_FUZZ;
    const uint8_t *selector = ERC20_SELECTORS_FUZZ[selector_idx];

    // Consume the selector choice byte
    data++;
    size--;

    // Initialize content from fuzzed data if available
    // ERC20 requires value to be zero
    if (size >= sizeof(txContent_t)) {
        memcpy(&content, data, sizeof(txContent_t));
        // ERC20 enforces that ETH amount should be 0
        memset(content.value.value, 0, sizeof(content.value.value));
    }

    // Setup init contract with the selected valid selector
    init_contract.interfaceVersion = ETH_PLUGIN_INTERFACE_VERSION_LATEST;
    init_contract.selector = selector;
    init_contract.txContent = &content;
    init_contract.pluginContext = plugin_context;
    init_contract.pluginContextLength = sizeof(plugin_context);
    init_contract.dataSize = size + SELECTOR_SIZE;

    erc20_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &init_contract);
    if (init_contract.result != ETH_PLUGIN_RESULT_OK) {
        return 0;
    }

    // Provide parameters - ERC20 expects specific parameter offsets
    size_t offset = 0;
    if (size >= sizeof(txContent_t)) {
        offset += sizeof(txContent_t);
    }

    uint32_t param_offset = SELECTOR_SIZE;
    while (size - offset >= PARAMETER_LENGTH) {
        provide_param.parameter = data + offset;
        provide_param.parameterOffset = param_offset;
        provide_param.parameter_size = PARAMETER_LENGTH;
        provide_param.pluginContext = plugin_context;
        provide_param.txContent = &content;

        erc20_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &provide_param);
        if (provide_param.result != ETH_PLUGIN_RESULT_OK) {
            return 0;
        }
        offset += PARAMETER_LENGTH;
        param_offset += PARAMETER_LENGTH;
    }

    // Finalize
    finalize.pluginContext = plugin_context;
    finalize.address = address;
    finalize.txContent = &content;

    erc20_plugin_call(ETH_PLUGIN_FINALIZE, &finalize);
    if (finalize.result != ETH_PLUGIN_RESULT_OK) {
        return 0;
    }

    // Provide token info if requested
    if (finalize.tokenLookup1 || finalize.tokenLookup2) {
        provide_info.pluginContext = plugin_context;
        provide_info.txContent = &content;
        if (finalize.tokenLookup1) {
            provide_info.item1 = &item1;
            // Set up mock token info
            strlcpy(item1.token.ticker, "FUZZ", MAX_TICKER_LEN);
            item1.token.decimals = 18;
        }
        if (finalize.tokenLookup2) {
            provide_info.item2 = &item2;
            strlcpy(item2.token.ticker, "FUZ2", MAX_TICKER_LEN);
            item2.token.decimals = 18;
        }

        erc20_plugin_call(ETH_PLUGIN_PROVIDE_INFO, &provide_info);
        if (provide_info.result != ETH_PLUGIN_RESULT_OK &&
            provide_info.result != ETH_PLUGIN_RESULT_FALLBACK) {
            return 0;
        }
    }

    // Query contract ID
    query_id.pluginContext = plugin_context;
    query_id.txContent = &content;
    query_id.name = name;
    query_id.nameLength = sizeof(name);
    query_id.version = version;
    query_id.versionLength = sizeof(version);

    erc20_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_ID, &query_id);
    if (query_id.result != ETH_PLUGIN_RESULT_OK) {
        return 0;
    }

    // Query contract UI for each screen
    uint8_t total_screens = finalize.numScreens + provide_info.additionalScreens;
    for (uint8_t i = 0; i < total_screens; i++) {
        query_ui.pluginContext = plugin_context;
        query_ui.txContent = &content;
        query_ui.title = title;
        query_ui.titleLength = sizeof(title);
        query_ui.msg = msg;
        query_ui.msgLength = sizeof(msg);
        query_ui.screenIndex = i;
        query_ui.item1 = finalize.tokenLookup1 ? &item1 : NULL;
        query_ui.item2 = finalize.tokenLookup2 ? &item2 : NULL;

        erc20_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &query_ui);
        if (query_ui.result != ETH_PLUGIN_RESULT_OK) {
            return 0;
        }
    }

    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    init_fuzzing_environment();
    fuzz_erc20_plugin(data, size);
    return 0;
}
