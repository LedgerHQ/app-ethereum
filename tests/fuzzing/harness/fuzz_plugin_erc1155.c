/**
 * Fuzzing harness for the ERC1155 internal plugin (NFT operations)
 *
 * Note: ERC1155 plugin requires NFT metadata to be set up (NO_NFT_METADATA check)
 * We populate tmpCtx.transactionContext.extraInfo[0] to bypass this check.
 *
 * The first byte of fuzz data selects which ERC1155 selector to test:
 * - 0: SET_APPROVAL_FOR_ALL (0xa22cb465)
 * - 1: SAFE_TRANSFER (0xf242432a)
 * - 2: SAFE_BATCH_TRANSFER (0x2eb2c2d6)
 */

#include "fuzz_utils.h"
#include "erc1155_plugin.h"
#include "shared_context.h"

// Buffer sizes for UI queries
#define NAME_LENGTH    32
#define VERSION_LENGTH 32
#define TITLE_LENGTH   32
#define MSG_LENGTH     79

// ERC1155 selectors
static const uint8_t ERC1155_APPROVE_FOR_ALL_SELECTOR[SELECTOR_SIZE] = {0xa2, 0x2c, 0xb4, 0x65};
static const uint8_t ERC1155_SAFE_TRANSFER_SELECTOR[SELECTOR_SIZE] = {0xf2, 0x42, 0x43, 0x2a};
static const uint8_t ERC1155_SAFE_BATCH_TRANSFER_SELECTOR[SELECTOR_SIZE] = {0x2e, 0xb2, 0xc2, 0xd6};

#define NUM_ERC1155_SELECTORS 3
static const uint8_t *const ERC1155_SELECTORS[NUM_ERC1155_SELECTORS] = {
    ERC1155_APPROVE_FOR_ALL_SELECTOR,
    ERC1155_SAFE_TRANSFER_SELECTOR,
    ERC1155_SAFE_BATCH_TRANSFER_SELECTOR,
};

static int fuzz_erc1155_plugin(const uint8_t *data, size_t size) {
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

    // Use first byte to select which ERC1155 selector to test
    uint8_t selector_idx = data[0] % NUM_ERC1155_SELECTORS;
    const uint8_t *selector = ERC1155_SELECTORS[selector_idx];

    // Consume the selector choice byte
    data++;
    size--;

    // Set up NFT metadata to bypass NO_NFT_METADATA check
    // The check is: allzeroes(&(tmpCtx.transactionContext.extraInfo[0]), sizeof(extraInfo_t))
    // We need to make this non-zero
    tmpCtx.transactionContext.extraInfo[0].nft.collectionName[0] = 'F';
    tmpCtx.transactionContext.extraInfo[0].nft.collectionName[1] = 'U';
    tmpCtx.transactionContext.extraInfo[0].nft.collectionName[2] = 'Z';
    tmpCtx.transactionContext.extraInfo[0].nft.collectionName[3] = 'Z';
    tmpCtx.transactionContext.extraInfo[0].nft.collectionName[4] = '\0';

    // Initialize content from fuzzed data if available
    if (size >= sizeof(txContent_t)) {
        memcpy(&content, data, sizeof(txContent_t));
    }

    // Setup init contract with the selected valid selector
    init_contract.interfaceVersion = ETH_PLUGIN_INTERFACE_VERSION_LATEST;
    init_contract.selector = selector;
    init_contract.txContent = &content;
    init_contract.pluginContext = plugin_context;
    init_contract.pluginContextLength = sizeof(plugin_context);
    init_contract.dataSize = size + SELECTOR_SIZE;

    erc1155_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &init_contract);
    if (init_contract.result != ETH_PLUGIN_RESULT_OK) {
        return 0;
    }

    // Provide parameters - ERC1155 expects specific parameter offsets
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

        erc1155_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &provide_param);
        if (provide_param.result < ETH_PLUGIN_RESULT_SUCCESSFUL) {
            return 0;
        }
        offset += PARAMETER_LENGTH;
        param_offset += PARAMETER_LENGTH;
    }

    // Finalize
    finalize.pluginContext = plugin_context;
    finalize.address = address;
    finalize.txContent = &content;

    erc1155_plugin_call(ETH_PLUGIN_FINALIZE, &finalize);
    if (finalize.result != ETH_PLUGIN_RESULT_OK) {
        return 0;
    }

    // Provide token info if requested
    if (finalize.tokenLookup1 || finalize.tokenLookup2) {
        provide_info.pluginContext = plugin_context;
        provide_info.txContent = &content;
        if (finalize.tokenLookup1) {
            provide_info.item1 = &item1;
            // Set up mock NFT info
            strlcpy(item1.nft.collectionName, "FuzzNFT", sizeof(item1.nft.collectionName));
        }
        if (finalize.tokenLookup2) {
            provide_info.item2 = &item2;
            strlcpy(item2.nft.collectionName, "FuzzNFT2", sizeof(item2.nft.collectionName));
        }

        erc1155_plugin_call(ETH_PLUGIN_PROVIDE_INFO, &provide_info);
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

    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_ID, &query_id);
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

        erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &query_ui);
        if (query_ui.result != ETH_PLUGIN_RESULT_OK) {
            return 0;
        }
    }

    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    init_fuzzing_environment();
    fuzz_erc1155_plugin(data, size);
    return 0;
}
