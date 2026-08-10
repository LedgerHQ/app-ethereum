/*
 * https://github.com/ethereum/EIPs/blob/master/EIPS/eip-7002.md
 */

#include "eip7002_plugin.h"
#include "common_utils.h"
#include "network.h"
#include "shared_context.h"
#include "utils.h"

static const uint8_t withdrawal_request_predeploy_address[ADDRESS_LENGTH] = {
    0x00, 0x00, 0x09, 0x61, 0xEf, 0x48, 0x0E, 0xb5, 0x5e, 0x80,
    0xD1, 0x9a, 0xd8, 0x35, 0x79, 0xA6, 0x4c, 0x00, 0x70, 0x02,
};

const uint8_t *const EIP7002_ADDRESSES[NUM_EIP7002_ADDRESSES] = {
    withdrawal_request_predeploy_address,
};

#define VALIDATOR_PUBKEY_SIZE   BLS12381_G1_COMPRESSED_PUBKEY_LENGTH
#define AMOUNT_SIZE             8
#define WITHDRAWAL_REQUEST_SIZE (VALIDATOR_PUBKEY_SIZE + AMOUNT_SIZE)
#define GWEI_TO_ETHER           9

typedef struct {
    union {
        uint8_t withdrawal_request[WITHDRAWAL_REQUEST_SIZE];
        struct {
            uint8_t validator_pubkey[VALIDATOR_PUBKEY_SIZE];
            uint8_t raw_amount[AMOUNT_SIZE];
        };
    };
    uint8_t received;
} eip7002_context_t;

static void eip7002_plugin_init_contract(ethPluginInitContract_t *param) {
    eip7002_context_t *context = (eip7002_context_t *) param->pluginContext;

    explicit_bzero(context, sizeof(*context));
    if ((size_t) context->received + CALLDATA_SELECTOR_SIZE > sizeof(context->withdrawal_request)) {
        param->result = ETH_PLUGIN_RESULT_ERROR;
    } else {
        memcpy(&context->withdrawal_request[context->received],
               param->selector,
               CALLDATA_SELECTOR_SIZE);
        context->received += CALLDATA_SELECTOR_SIZE;
        param->result = ETH_PLUGIN_RESULT_OK;
    }
}

static void eip7002_plugin_provider_parameter(ethPluginProvideParameter_t *param) {
    eip7002_context_t *context = (eip7002_context_t *) param->pluginContext;

    if ((context->received + param->parameter_size) > sizeof(context->withdrawal_request)) {
        param->result = ETH_PLUGIN_RESULT_ERROR;
    } else {
        memcpy(&context->withdrawal_request[context->received],
               param->parameter,
               param->parameter_size);
        context->received += param->parameter_size;
        param->result = ETH_PLUGIN_RESULT_OK;
    }
}

// EIP-7002 charges a dynamic per-request fee paid in native value. In normal
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

static void eip7002_plugin_finalize(ethPluginFinalize_t *param) {
    eip7002_context_t *context = (eip7002_context_t *) param->pluginContext;

    if (get_tx_chain_id() != ETHEREUM_MAINNET_CHAINID) {
        PRINTF("eip7002: withdrawal predeploy only valid on Ethereum mainnet\n");
        param->result = ETH_PLUGIN_RESULT_ERROR;
        return;
    }
    param->uiType = ETH_UI_TYPE_GENERIC;
    // Validator screen is always shown. The native tx.value is shown only
    // when it exceeds the dust threshold defined by has_tx_value, so a
    // hostile dApp cannot smuggle a meaningful ETH/native value into a
    // staking-style request that otherwise only renders calldata.
    param->numScreens = 1;
    if (has_tx_value(param->txContent)) {
        param->numScreens++;
    }
    if (!is_zeroes_buffer(context->raw_amount, sizeof(context->raw_amount))) {
        param->numScreens++;
    }
    param->result = (context->received == sizeof(context->withdrawal_request))
                        ? ETH_PLUGIN_RESULT_OK
                        : ETH_PLUGIN_RESULT_ERROR;
}

static void eip7002_plugin_query_contract_id(ethQueryContractID_t *param) {
    eip7002_context_t *context = (eip7002_context_t *) param->pluginContext;

    strlcpy(param->version, "do", param->versionLength);
    if (is_zeroes_buffer(context->raw_amount, sizeof(context->raw_amount))) {
        strlcpy(param->name, "full exit", param->nameLength);
    } else {
        strlcpy(param->name, "partial withdrawal", param->nameLength);
    }
    param->result = ETH_PLUGIN_RESULT_OK;
}

static void eip7002_plugin_query_contract_ui(ethQueryContractUI_t *param) {
    eip7002_context_t *context = (eip7002_context_t *) param->pluginContext;
    uint64_t chain_id = get_tx_chain_id();
    const char *ticker = get_displayable_ticker(&chain_id, g_chain_config, true);
    // Map a screen index to a logical screen kind based on which optional
    // screens are present for this transaction.
    bool show_tx_value = has_tx_value(param->txContent);
    bool show_request_amount = !is_zeroes_buffer(context->raw_amount, sizeof(context->raw_amount));
    uint8_t idx = param->screenIndex;
    enum { S_VALIDATOR, S_TX_VALUE, S_REQUEST_AMOUNT, S_UNKNOWN } screen = S_UNKNOWN;

    if (idx == 0) {
        screen = S_VALIDATOR;
    } else if (show_tx_value && idx == 1) {
        screen = S_TX_VALUE;
    } else if (show_request_amount && idx == (show_tx_value ? 2 : 1)) {
        screen = S_REQUEST_AMOUNT;
    }

    switch (screen) {
        case S_VALIDATOR:
            if (param->msgLength < 2) {
                return;
            }
            strlcpy(param->title, "Validator", param->titleLength);
            memcpy(param->msg, "0x", 2);
            format_hex(context->validator_pubkey,
                       sizeof(context->validator_pubkey),
                       &param->msg[2],
                       param->msgLength - 2);
            break;
        case S_TX_VALUE:
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
        case S_REQUEST_AMOUNT:
            strlcpy(param->title, "Amount", param->titleLength);
            if (!amountToString(context->raw_amount,
                                sizeof(context->raw_amount),
                                GWEI_TO_ETHER,
                                ticker,
                                param->msg,
                                param->msgLength)) {
                param->result = ETH_PLUGIN_RESULT_ERROR;
                return;
            }
            break;
        case S_UNKNOWN:
            break;
    }
    param->result = ETH_PLUGIN_RESULT_OK;
}

void eip7002_plugin_call(eth_plugin_msg_t msg, void *param) {
    if (param != NULL) {
        switch (msg) {
            case ETH_PLUGIN_INIT_CONTRACT:
                eip7002_plugin_init_contract(param);
                break;
            case ETH_PLUGIN_PROVIDE_PARAMETER:
                eip7002_plugin_provider_parameter(param);
                break;
            case ETH_PLUGIN_FINALIZE:
                eip7002_plugin_finalize(param);
                break;
            case ETH_PLUGIN_QUERY_CONTRACT_ID:
                eip7002_plugin_query_contract_id(param);
                break;
            case ETH_PLUGIN_QUERY_CONTRACT_UI:
                eip7002_plugin_query_contract_ui(param);
                break;
            default:
                PRINTF("Unhandled message 0x%x\n", msg);
        }
    }
}
