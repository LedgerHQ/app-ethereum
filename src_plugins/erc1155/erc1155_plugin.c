#ifdef HAVE_NFT_SUPPORT

#include <string.h>
#include "erc1155_plugin.h"
#include "eth_plugin_internal.h"
#include "ethUtils.h"
#include "eth_plugin_handler.h"

static const uint8_t ERC1155_APPROVE_FOR_ALL_SELECTOR[SELECTOR_SIZE] = {0xa2, 0x2c, 0xb4, 0x65};
static const uint8_t ERC1155_SAFE_TRANSFER_SELECTOR[SELECTOR_SIZE] = {0xf2, 0x42, 0x43, 0x2a};
static const uint8_t ERC1155_SAFE_BATCH_TRANSFER[SELECTOR_SIZE] = {0x2e, 0xb2, 0xc2, 0xd6};

const uint8_t *const ERC1155_SELECTORS[SELECTORS_COUNT] = {
    ERC1155_APPROVE_FOR_ALL_SELECTOR,
    ERC1155_SAFE_TRANSFER_SELECTOR,
    ERC1155_SAFE_BATCH_TRANSFER,
};

static void handle_init_contract(void *parameters) {
    ethPluginInitContract_t *msg = (ethPluginInitContract_t *) parameters;
    erc1155_context_t *context = (erc1155_context_t *) msg->pluginContext;

    if (NO_NFT_METADATA) {
        PRINTF("No NFT metadata when trying to sign!\n");
        msg->result = ETH_PLUGIN_RESULT_ERROR;
        return;
    }
    uint8_t i;
    for (i = 0; i < SELECTORS_COUNT; i++) {
        if (memcmp((uint8_t *) PIC(ERC1155_SELECTORS[i]), msg->selector, SELECTOR_SIZE) == 0) {
            context->selectorIndex = i;
            break;
        }
    }

    // No selector found.
    if (i == SELECTORS_COUNT) {
        PRINTF("Unknown erc1155 selector %.*H\n", SELECTOR_SIZE, msg->selector);
        msg->result = ETH_PLUGIN_RESULT_FALLBACK;
        return;
    }

    msg->result = ETH_PLUGIN_RESULT_OK;
    switch (context->selectorIndex) {
        case SAFE_TRANSFER:
        case SAFE_BATCH_TRANSFER:
            context->next_param = FROM;
            break;
        case SET_APPROVAL_FOR_ALL:
            context->next_param = OPERATOR;
            break;
        default:
            PRINTF("Unsupported selector index: %d\n", context->selectorIndex);
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

static void handle_finalize(void *parameters) {
    ethPluginFinalize_t *msg = (ethPluginFinalize_t *) parameters;
    erc1155_context_t *context = (erc1155_context_t *) msg->pluginContext;

    if (context->selectorIndex != SAFE_BATCH_TRANSFER) {
        msg->tokenLookup1 = msg->pluginSharedRO->txContent->destination;
    } else {
        msg->tokenLookup1 = NULL;
    }

    msg->tokenLookup2 = NULL;
    switch (context->selectorIndex) {
        case SAFE_TRANSFER:
            msg->numScreens = 5;
            break;
        case SAFE_BATCH_TRANSFER:
            msg->numScreens = 4;
            break;
        case SET_APPROVAL_FOR_ALL:
            msg->numScreens = 3;
            break;
        default:
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            return;
    }
    // Check if some ETH is attached to this tx
    if (!allzeroes((void *) &msg->pluginSharedRO->txContent->value,
                   sizeof(msg->pluginSharedRO->txContent->value))) {
        // Those functions are not payable so return an error.
        msg->result = ETH_PLUGIN_RESULT_ERROR;
        return;
    }
    msg->uiType = ETH_UI_TYPE_GENERIC;
    msg->result = ETH_PLUGIN_RESULT_OK;
}

static void handle_provide_info(void *parameters) {
    ethPluginProvideInfo_t *msg = (ethPluginProvideInfo_t *) parameters;

    msg->result = ETH_PLUGIN_RESULT_OK;
}

static void handle_query_contract_id(void *parameters) {
    ethQueryContractID_t *msg = (ethQueryContractID_t *) parameters;
    erc1155_context_t *context = (erc1155_context_t *) msg->pluginContext;

    msg->result = ETH_PLUGIN_RESULT_OK;

    strlcpy(msg->name, "NFT", msg->nameLength);

    switch (context->selectorIndex) {
        case SET_APPROVAL_FOR_ALL:
            strlcpy(msg->version, "Allowance", msg->versionLength);
            break;
        case SAFE_TRANSFER:
            strlcpy(msg->version, "Transfer", msg->versionLength);
            break;
        case SAFE_BATCH_TRANSFER:
            strlcpy(msg->version, "Batch Transfer", msg->versionLength);
            break;
        default:
            PRINTF("Unsupported selector %d\n", context->selectorIndex);
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

void erc1155_plugin_call(int message, void *parameters) {
    switch (message) {
        case ETH_PLUGIN_INIT_CONTRACT: {
            handle_init_contract(parameters);
        } break;
        case ETH_PLUGIN_PROVIDE_PARAMETER: {
            handle_provide_parameter_1155(parameters);
        } break;
        case ETH_PLUGIN_FINALIZE: {
            handle_finalize(parameters);
        } break;
        case ETH_PLUGIN_PROVIDE_INFO: {
            handle_provide_info(parameters);
        } break;
        case ETH_PLUGIN_QUERY_CONTRACT_ID: {
            handle_query_contract_id(parameters);
        } break;
        case ETH_PLUGIN_QUERY_CONTRACT_UI: {
            handle_query_contract_ui_1155(parameters);
        } break;
        default:
            PRINTF("Unhandled message %d\n", message);
            break;
    }
}

#endif  // HAVE_NFT_SUPPORT
