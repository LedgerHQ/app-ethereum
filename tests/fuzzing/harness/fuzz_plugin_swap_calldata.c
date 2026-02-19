/**
 * Fuzzing harness for the swap_with_calldata internal plugin
 *
 * Note: This plugin requires swap mode context to be set up:
 * - G_called_from_swap = true
 * - G_swap_mode = SWAP_MODE_CROSSCHAIN_PENDING_CHECK
 * - G_swap_crosschain_hash must point to a valid hash for comparison
 */

#include "fuzz_utils.h"
#include "shared_context.h"
#include "swap_lib_calls.h"

// Forward declaration of the plugin call function
void swap_with_calldata_plugin_call(eth_plugin_msg_t message, void *parameters);

// Buffer sizes
#define HASH_SIZE 32

static int fuzz_swap_calldata_plugin(const uint8_t *data, size_t size) {
    ethPluginInitContract_t init_contract = {0};
    ethPluginProvideParameter_t provide_param = {0};
    ethPluginFinalize_t finalize = {0};
    txContent_t content = {0};

    uint8_t plugin_context[PLUGIN_CONTEXT_SIZE] = {0};

    const uint8_t address[ADDRESS_LENGTH] = {0};

    // Need at least selector (4 bytes) + some data for hash computation
    if (size < SELECTOR_SIZE) {
        return 0;
    }

    // Set up swap mode context
    G_called_from_swap = true;
    G_swap_mode = SWAP_MODE_CROSSCHAIN_PENDING_CHECK;

    // Allocate and set up a mock crosschain hash
    // The plugin will compute hash of (selector + parameters) and compare to this
    // For fuzzing, we can either:
    // 1. Use fuzzed data as the expected hash (will mostly fail comparison)
    // 2. Accept that hash mismatches will set G_swap_mode = SWAP_MODE_ERROR
    // We go with option 1 to still exercise the code paths
    uint8_t mock_hash[HASH_SIZE] = {0};
    if (size >= SELECTOR_SIZE + HASH_SIZE) {
        // Use part of fuzzed data as the expected hash
        memcpy(mock_hash, data + SELECTOR_SIZE, HASH_SIZE);
    }
    G_swap_crosschain_hash = mock_hash;

    // Initialize content from fuzzed data if available
    size_t content_offset = SELECTOR_SIZE + HASH_SIZE;
    if (size >= content_offset + sizeof(txContent_t)) {
        memcpy(&content, data + content_offset, sizeof(txContent_t));
    }

    // Setup init contract
    init_contract.interfaceVersion = ETH_PLUGIN_INTERFACE_VERSION_LATEST;
    init_contract.selector = data;
    init_contract.txContent = &content;
    init_contract.pluginContext = plugin_context;
    init_contract.pluginContextLength = sizeof(plugin_context);
    init_contract.dataSize = size;

    swap_with_calldata_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &init_contract);
    if (init_contract.result != ETH_PLUGIN_RESULT_OK) {
        goto cleanup;
    }

    // Provide parameters
    size_t offset = content_offset;
    if (size >= content_offset + sizeof(txContent_t)) {
        offset += sizeof(txContent_t);
    }

    uint32_t param_offset = SELECTOR_SIZE;
    while (offset + PARAMETER_LENGTH <= size) {
        provide_param.parameter = data + offset;
        provide_param.parameterOffset = param_offset;
        provide_param.parameter_size = PARAMETER_LENGTH;
        provide_param.pluginContext = plugin_context;
        provide_param.txContent = &content;

        swap_with_calldata_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &provide_param);
        if (provide_param.result != ETH_PLUGIN_RESULT_OK) {
            goto cleanup;
        }
        offset += PARAMETER_LENGTH;
        param_offset += PARAMETER_LENGTH;
    }

    // Finalize - this is where hash comparison happens
    finalize.pluginContext = plugin_context;
    finalize.address = address;
    finalize.txContent = &content;

    swap_with_calldata_plugin_call(ETH_PLUGIN_FINALIZE, &finalize);
    // Note: This plugin returns FALLBACK on success to let ETH app handle display
    // It modifies G_swap_mode based on hash comparison result

cleanup:
    // Reset swap context
    G_called_from_swap = false;
    G_swap_mode = SWAP_MODE_STANDARD;
    G_swap_crosschain_hash = NULL;

    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    init_fuzzing_environment();
    fuzz_swap_calldata_plugin(data, size);
    return 0;
}
