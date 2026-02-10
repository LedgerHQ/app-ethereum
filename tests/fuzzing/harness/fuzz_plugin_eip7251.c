/**
 * Fuzzing harness for the EIP-7251 internal plugin (consolidation requests)
 */

#include "fuzz_utils.h"
#include "eip7251_plugin.h"

// Buffer sizes for UI queries
#define NAME_LENGTH    32
#define VERSION_LENGTH 32
#define TITLE_LENGTH   32
#define MSG_LENGTH     79

static int fuzz_eip7251_plugin(const uint8_t *data, size_t size) {
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

    // Need at least selector (4 bytes)
    if (size < SELECTOR_SIZE) {
        return 0;
    }

    // Initialize content from fuzzed data if available
    if (size >= SELECTOR_SIZE + sizeof(txContent_t)) {
        memcpy(&content, data + SELECTOR_SIZE, sizeof(txContent_t));
    }

    // Setup init contract
    init_contract.interfaceVersion = ETH_PLUGIN_INTERFACE_VERSION_LATEST;
    init_contract.selector = data;
    init_contract.txContent = &content;
    init_contract.pluginContext = plugin_context;
    init_contract.pluginContextLength = sizeof(plugin_context);
    init_contract.dataSize = size;

    eip7251_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &init_contract);
    if (init_contract.result != ETH_PLUGIN_RESULT_OK) {
        return 0;
    }

    // Provide parameters
    size_t offset = SELECTOR_SIZE;
    if (size >= SELECTOR_SIZE + sizeof(txContent_t)) {
        offset += sizeof(txContent_t);
    }

    while (size - offset >= PARAMETER_LENGTH) {
        provide_param.parameter = data + offset;
        provide_param.parameterOffset = offset;
        provide_param.parameter_size = PARAMETER_LENGTH;
        provide_param.pluginContext = plugin_context;
        provide_param.txContent = &content;

        eip7251_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &provide_param);
        if (provide_param.result != ETH_PLUGIN_RESULT_OK) {
            return 0;
        }
        offset += PARAMETER_LENGTH;
    }

    // Handle remaining bytes if any (last parameter may be smaller)
    if (size - offset > 0) {
        provide_param.parameter = data + offset;
        provide_param.parameterOffset = offset;
        provide_param.parameter_size = size - offset;
        provide_param.pluginContext = plugin_context;
        provide_param.txContent = &content;

        eip7251_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &provide_param);
        if (provide_param.result != ETH_PLUGIN_RESULT_OK) {
            return 0;
        }
    }

    // Finalize
    finalize.pluginContext = plugin_context;
    finalize.address = address;
    finalize.txContent = &content;

    eip7251_plugin_call(ETH_PLUGIN_FINALIZE, &finalize);
    if (finalize.result != ETH_PLUGIN_RESULT_OK) {
        return 0;
    }

    // Provide token info if requested
    if (finalize.tokenLookup1 || finalize.tokenLookup2) {
        provide_info.pluginContext = plugin_context;
        provide_info.txContent = &content;
        if (finalize.tokenLookup1) {
            provide_info.item1 = &item1;
        }
        if (finalize.tokenLookup2) {
            provide_info.item2 = &item2;
        }

        eip7251_plugin_call(ETH_PLUGIN_PROVIDE_INFO, &provide_info);
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

    eip7251_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_ID, &query_id);
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

        eip7251_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &query_ui);
        if (query_ui.result != ETH_PLUGIN_RESULT_OK) {
            return 0;
        }
    }

    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    init_fuzzing_environment();
    fuzz_eip7251_plugin(data, size);
    return 0;
}
