/*
 * https://github.com/ethereum/EIPs/blob/master/EIPS/eip-7251.md
 */

#include "eip7251_plugin.h"
#include "common_utils.h"
#include "network.h"
#include "shared_context.h"
#include "utils.h"

static const uint8_t consolidation_request_predeploy_address[ADDRESS_LENGTH] = {
    0x00, 0x00, 0xBB, 0xdD, 0xc7, 0xCE, 0x48, 0x86, 0x42, 0xfb,
    0x57, 0x9F, 0x8B, 0x00, 0xf3, 0xa5, 0x90, 0x00, 0x72, 0x51,
};

const uint8_t *const EIP7251_ADDRESSES[NUM_EIP7251_ADDRESSES] = {
    consolidation_request_predeploy_address,
};

#define VALIDATOR_PUBKEY_SIZE      BLS12381_G1_COMPRESSED_PUBKEY_LENGTH
#define CONSOLIDATION_REQUEST_SIZE (VALIDATOR_PUBKEY_SIZE * 2)

typedef struct {
    union {
        uint8_t consolidation_request[CONSOLIDATION_REQUEST_SIZE];
        struct {
            uint8_t source_pubkey[VALIDATOR_PUBKEY_SIZE];
            uint8_t target_pubkey[VALIDATOR_PUBKEY_SIZE];
        };
    };
    uint8_t received;
} eip7251_context_t;

static bool target_equals_source(const eip7251_context_t *ctx) {
    return memcmp(ctx->source_pubkey, ctx->target_pubkey, VALIDATOR_PUBKEY_SIZE) == 0;
}

static void eip7251_plugin_init_contract(ethPluginInitContract_t *param) {
    eip7251_context_t *context = (eip7251_context_t *) param->pluginContext;

    explicit_bzero(context, sizeof(*context));
    if ((size_t) context->received + CALLDATA_SELECTOR_SIZE >
        sizeof(context->consolidation_request)) {
        param->result = ETH_PLUGIN_RESULT_ERROR;
    } else {
        memcpy(&context->consolidation_request[context->received],
               param->selector,
               CALLDATA_SELECTOR_SIZE);
        context->received += CALLDATA_SELECTOR_SIZE;
        param->result = ETH_PLUGIN_RESULT_OK;
    }
}

static void eip7251_plugin_provider_parameter(ethPluginProvideParameter_t *param) {
    eip7251_context_t *context = (eip7251_context_t *) param->pluginContext;

    if ((context->received + param->parameter_size) > sizeof(context->consolidation_request)) {
        param->result = ETH_PLUGIN_RESULT_ERROR;
    } else {
        memcpy(&context->consolidation_request[context->received],
               param->parameter,
               param->parameter_size);
        context->received += param->parameter_size;
        param->result = ETH_PLUGIN_RESULT_OK;
    }
}

// EIP-7251 charges a dynamic per-request fee paid in native value. In normal
// operation this fee is in the wei-to-gwei range — showing a "Tx value: 1 wei"
// screen on every legitimate request is noisy without informing the user.
// Hide the screen as long as the value stays below this threshold; anything
// above is worth surfacing because it dwarfs the protocol fee. The attacker
// budget for a hidden value is therefore capped at TX_VALUE_MIN_DISPLAY_WEI,
// i.e. dust ($0.000004 at $4000/ETH), instead of the unbounded original CVE.
#define TX_VALUE_MIN_DISPLAY_WEI 1000000000ULL  // 1 gwei

// Whether the transaction carries a native value worth surfacing to the user.
// NULL-safe so callers can pass param->txContent directly from either the
// ETH_PLUGIN_FINALIZE or ETH_PLUGIN_QUERY_CONTRACT_UI message structs.
static bool has_tx_value(const txContent_t *txContent) {
    if ((txContent == NULL) || (txContent->value.length == 0)) {
        return false;
    }
    if (txContent->value.length > sizeof(uint64_t)) {
        // > 2^64 wei is unambiguously larger than the threshold.
        return true;
    }
    uint64_t val = 0;
    for (uint8_t i = 0; i < txContent->value.length; ++i) {
        val = (val << 8) | txContent->value.value[i];
    }
    return val > TX_VALUE_MIN_DISPLAY_WEI;
}

static void eip7251_plugin_finalize(ethPluginFinalize_t *param) {
    eip7251_context_t *context = (eip7251_context_t *) param->pluginContext;

    param->uiType = ETH_UI_TYPE_GENERIC;
    // Source validator is always shown. Target is shown when distinct. The
    // native tx.value is shown only when it exceeds the dust threshold
    // defined by has_tx_value, so a hostile dApp cannot smuggle a meaningful
    // ETH/native value into a consolidation request that otherwise only
    // renders validator pubkeys.
    param->numScreens = 1;
    if (!target_equals_source(context)) {
        param->numScreens++;
    }
    if (has_tx_value(param->txContent)) {
        param->numScreens++;
    }
    param->result = (context->received == sizeof(context->consolidation_request))
                        ? ETH_PLUGIN_RESULT_OK
                        : ETH_PLUGIN_RESULT_ERROR;
}

static void eip7251_plugin_query_contract_id(ethQueryContractID_t *param) {
    if (target_equals_source((eip7251_context_t *) param->pluginContext)) {
        strlcpy(param->version, "compound", param->versionLength);
    } else {
        strlcpy(param->version, "consolidate", param->versionLength);
    }
    strlcpy(param->name, "stake", param->nameLength);
    param->result = ETH_PLUGIN_RESULT_OK;
}

static void eip7251_plugin_query_contract_ui(ethQueryContractUI_t *param) {
    eip7251_context_t *context = (eip7251_context_t *) param->pluginContext;
    // Map a screen index to a logical screen kind based on which optional
    // screens are present for this transaction.
    bool show_target = !target_equals_source(context);
    bool show_tx_value = has_tx_value(param->txContent);
    uint8_t idx = param->screenIndex;
    enum { S_SOURCE, S_TARGET, S_TX_VALUE, S_UNKNOWN } screen = S_UNKNOWN;

    if (idx == 0) {
        screen = S_SOURCE;
    } else if (show_target && idx == 1) {
        screen = S_TARGET;
    } else if (show_tx_value && idx == (show_target ? 2 : 1)) {
        screen = S_TX_VALUE;
    }

    if (param->msgLength < 2) {
        return;
    }
    switch (screen) {
        case S_SOURCE:
            memcpy(param->msg, "0x", 2);
            strlcpy(param->title, show_target ? "From validator" : "Validator", param->titleLength);
            format_hex(context->source_pubkey,
                       sizeof(context->source_pubkey),
                       &param->msg[2],
                       param->msgLength - 2);
            break;
        case S_TARGET:
            memcpy(param->msg, "0x", 2);
            strlcpy(param->title, "To validator", param->titleLength);
            format_hex(context->target_pubkey,
                       sizeof(context->target_pubkey),
                       &param->msg[2],
                       param->msgLength - 2);
            break;
        case S_TX_VALUE: {
            uint64_t chain_id = get_tx_chain_id();
            const char *ticker = get_displayable_ticker(&chain_id, g_chain_config);
            strlcpy(param->title, "Tx value", param->titleLength);
            if (!amountToString(param->txContent->value.value,
                                param->txContent->value.length,
                                WEI_TO_ETHER,
                                ticker,
                                param->msg,
                                param->msgLength)) {
                param->result = ETH_PLUGIN_RESULT_ERROR;
                return;
            }
            break;
        }
        case S_UNKNOWN:
            return;
    }
    param->result = ETH_PLUGIN_RESULT_OK;
}

void eip7251_plugin_call(eth_plugin_msg_t msg, void *param) {
    if (param != NULL) {
        switch (msg) {
            case ETH_PLUGIN_INIT_CONTRACT:
                eip7251_plugin_init_contract(param);
                break;
            case ETH_PLUGIN_PROVIDE_PARAMETER:
                eip7251_plugin_provider_parameter(param);
                break;
            case ETH_PLUGIN_FINALIZE:
                eip7251_plugin_finalize(param);
                break;
            case ETH_PLUGIN_QUERY_CONTRACT_ID:
                eip7251_plugin_query_contract_id(param);
                break;
            case ETH_PLUGIN_QUERY_CONTRACT_UI:
                eip7251_plugin_query_contract_ui(param);
                break;
            default:
                PRINTF("Unhandled message 0x%x\n", msg);
        }
    }
}
