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

#define VALIDATOR_PUBKEY_SIZE   48
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
    memcpy(&context->withdrawal_request[context->received], param->selector, SELECTOR_SIZE);
    context->received += SELECTOR_SIZE;
    param->result = ETH_PLUGIN_RESULT_OK;
}

static void eip7002_plugin_provider_parameter(ethPluginProvideParameter_t *param) {
    eip7002_context_t *context = (eip7002_context_t *) param->pluginContext;

    if ((context->received + param->parameter_size) > sizeof(context->withdrawal_request)) {
        param->result = ETH_PLUGIN_RESULT_ERROR;
    }
    memcpy(&context->withdrawal_request[context->received],
           param->parameter,
           param->parameter_size);
    context->received += param->parameter_size;
    param->result = ETH_PLUGIN_RESULT_OK;
}

static void eip7002_plugin_finalize(ethPluginFinalize_t *param) {
    eip7002_context_t *context = (eip7002_context_t *) param->pluginContext;

    param->uiType = ETH_UI_TYPE_GENERIC;
    param->numScreens = allzeroes(context->raw_amount, sizeof(context->raw_amount)) ? 1 : 2;
    param->result = (context->received == sizeof(context->withdrawal_request))
                        ? ETH_PLUGIN_RESULT_OK
                        : ETH_PLUGIN_RESULT_ERROR;
}

static void eip7002_plugin_query_contract_id(ethQueryContractID_t *param) {
    eip7002_context_t *context = (eip7002_context_t *) param->pluginContext;

    strlcpy(param->version, "do", param->versionLength);
    if (allzeroes(context->raw_amount, sizeof(context->raw_amount))) {
        strlcat(param->name, "full exit", param->nameLength);
    } else {
        strlcat(param->name, "partial withdrawal", param->nameLength);
    }
    param->result = ETH_PLUGIN_RESULT_OK;
}

static void eip7002_plugin_query_contract_ui(ethQueryContractUI_t *param) {
    eip7002_context_t *context = (eip7002_context_t *) param->pluginContext;
    uint64_t chain_id = get_tx_chain_id();
    const char *ticker = get_displayable_ticker(&chain_id, chainConfig, true);

    switch (param->screenIndex) {
        case 0:
            strlcpy(param->title, "Validator", param->titleLength);
            memcpy(param->msg, "0x", 2);
            format_hex(context->validator_pubkey,
                       sizeof(context->validator_pubkey),
                       &param->msg[2],
                       param->msgLength - 2);
            break;
        case 1:
            strlcpy(param->title, "Amount", param->titleLength);
            if (!amountToString(context->raw_amount,
                                sizeof(context->raw_amount),
                                GWEI_TO_ETHER,
                                ticker,
                                param->msg,
                                param->msgLength)) {
                param->result = ETH_PLUGIN_RESULT_ERROR;
                break;
            }
            break;
        default:
            break;
    }
    param->result = ETH_PLUGIN_RESULT_OK;
}

void eip7002_plugin_call(eth_plugin_msg_t msg, void *param) {
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
