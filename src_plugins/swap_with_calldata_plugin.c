#include <string.h>
#include "lib_cxng/src/cx_sha256.h"
#include "plugin_utils.h"
#include "eth_plugin_internal.h"
#include "eth_plugin_interface.h"
#include "eth_plugin_handler.h"

/* SWAP_WITH_CALLDATA 'internal plugin'
 *
 * This internal plugin is used to parse calldata in the SWAP_WITH_CALLDATA context.
 * Used for example by Thorswap and LiFi.
 * Difference with main swap mode:
 * - The swap transaction promise made by the partner contains a 32 bytes hash additional field
 * - This hash is the hash of the calldata of the final signing transaction
 * - During the final TX signature in ETH, we check the destination and amount as usual
 * - We also check the hash of the calldata against the promised hash.
 *
 * This minimalist internal plugin is used to do this calldata hashing and check.
 */

typedef struct swap_with_calldata_context_s {
    // Hash of the calldata received.
    // Computed on (Selector + N * 32bytes parameters)
    // We don't care at all about the content, we'll just compare the hash with the promised hash.
    cx_sha256_t update_hash;
} swap_with_calldata_context_t;

void handle_init_contract_swap_with_calldata(ethPluginInitContract_t *msg) {
    PRINTF("handle_init_contract_swap_with_calldata\n");
    if (!G_called_from_swap) {
        // Can't happen in theory, but let's double check.
        PRINTF("swap_with_calldata plugin can't be used outside of SWAP context");
        msg->result = ETH_PLUGIN_RESULT_ERROR;
        return;
    }

    swap_with_calldata_context_t *context = (swap_with_calldata_context_t *) msg->pluginContext;

    PRINTF("swap_with_calldata plugin init contract %.*H\n", SELECTOR_SIZE, msg->selector);

    // Can't fail
    cx_sha256_init_no_throw(&context->update_hash);

    if (cx_sha256_update(&context->update_hash, msg->selector, SELECTOR_SIZE) != CX_OK) {
        PRINTF("ERROR: cx_sha256_update on selector\n");
        msg->result = ETH_PLUGIN_RESULT_ERROR;
    } else {
        msg->result = ETH_PLUGIN_RESULT_OK;
    }
}

void handle_provide_parameter_swap_with_calldata(ethPluginProvideParameter_t *msg) {
    PRINTF("handle_provide_parameter_swap_with_calldata\n");
    if (!G_called_from_swap) {
        // Can't happen in theory, but let's double check.
        PRINTF("swap_with_calldata plugin can't be used outside of SWAP context");
        msg->result = ETH_PLUGIN_RESULT_ERROR;
        return;
    }

    swap_with_calldata_context_t *context = (swap_with_calldata_context_t *) msg->pluginContext;

    PRINTF("swap_with_calldata plugin provide parameter %d %.*H\n",
           msg->parameterOffset,
           PARAMETER_LENGTH,
           msg->parameter);

    if (cx_sha256_update(&context->update_hash, msg->parameter, PARAMETER_LENGTH) != CX_OK) {
        PRINTF("ERROR: cx_sha256_update on parameter %d\n", msg->parameterOffset);
        msg->result = ETH_PLUGIN_RESULT_ERROR;
    } else {
        msg->result = ETH_PLUGIN_RESULT_OK;
    }
}

void handle_finalize_swap_with_calldata(ethPluginFinalize_t *msg) {
    PRINTF("handle_finalize_swap_with_calldata\n");
    if (!G_called_from_swap) {
        // Can't happen in theory, but let's double check.
        PRINTF("swap_with_calldata plugin can't be used outside of SWAP context");
        msg->result = ETH_PLUGIN_RESULT_ERROR;
        return;
    }

    swap_with_calldata_context_t *context = (swap_with_calldata_context_t *) msg->pluginContext;

    // Can't fail
    uint8_t hash_digest[CX_SHA256_SIZE];
    cx_sha256_final(&context->update_hash, hash_digest);

    // Compare computed hash with promise made in Exchange by the partner
    if (memcmp(hash_digest, G_swap_crosschain_hash, CX_SHA256_SIZE) != 0) {
        PRINTF("ERROR: Wrong hash, promised %.*H, received %.*H\n",
               CX_SHA256_SIZE,
               G_swap_crosschain_hash,
               CX_SHA256_SIZE,
               hash_digest);
        G_swap_mode = SWAP_MODE_ERROR;
    } else {
        PRINTF("Finalize successful, hashes match\n");
        if (G_swap_mode == SWAP_MODE_CROSSCHAIN_PENDING_CHECK) {
            // Remember that a calldata exists and is valid
            // This differentiates the case were there was no calldata is provided
            G_swap_mode = SWAP_MODE_CROSSCHAIN_SUCCESS;
        } else {
            PRINTF("G_swap_mode %d wrong, refusing to validate the calldata", G_swap_mode);
            G_swap_mode = SWAP_MODE_ERROR;
        }
    }

    // We use fallback so that the Eth UI takes back the display
    // This is simpler than forwarding the values in txContent
    // We return this even in case of errors because the error handling is done with G_swap_mode
    msg->result = ETH_PLUGIN_RESULT_FALLBACK;
    msg->tokenLookup1 = NULL;
    msg->tokenLookup2 = NULL;
}

void swap_with_calldata_plugin_call(int message, void *parameters) {
    switch (message) {
        case ETH_PLUGIN_INIT_CONTRACT:
            handle_init_contract_swap_with_calldata((ethPluginInitContract_t *) parameters);
            break;
        case ETH_PLUGIN_PROVIDE_PARAMETER:
            handle_provide_parameter_swap_with_calldata((ethPluginProvideParameter_t *) parameters);
            break;
        case ETH_PLUGIN_FINALIZE:
            handle_finalize_swap_with_calldata((ethPluginFinalize_t *) parameters);
            break;
        case ETH_PLUGIN_PROVIDE_INFO:
        case ETH_PLUGIN_QUERY_CONTRACT_ID:
        case ETH_PLUGIN_QUERY_CONTRACT_UI:
        default:
            PRINTF("Unhandled message %d\n", message);
            break;
    }
}
