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

#define VALIDATOR_PUBKEY_SIZE      48
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
    if ((context->received + CALLDATA_SELECTOR_SIZE) > sizeof(context->consolidation_request)) {
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

static void eip7251_plugin_finalize(ethPluginFinalize_t *param) {
    eip7251_context_t *context = (eip7251_context_t *) param->pluginContext;

    param->uiType = ETH_UI_TYPE_GENERIC;
    param->numScreens = target_equals_source(context) ? 1 : 2;
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

    if (param->msgLength >= 2) {
        memcpy(param->msg, "0x", 2);
        switch (param->screenIndex) {
            case 0:
                if (target_equals_source(context)) {
                    strlcpy(param->title, "Validator", param->titleLength);
                } else {
                    strlcpy(param->title, "From validator", param->titleLength);
                }
                format_hex(context->source_pubkey,
                           sizeof(context->source_pubkey),
                           &param->msg[2],
                           param->msgLength - 2);
                break;
            case 1:
                strlcpy(param->title, "To validator", param->titleLength);
                format_hex(context->target_pubkey,
                           sizeof(context->target_pubkey),
                           &param->msg[2],
                           param->msgLength - 2);
                break;
            default:
                break;
        }
        param->result = ETH_PLUGIN_RESULT_OK;
    }
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
