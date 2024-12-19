#ifdef HAVE_NFT_SUPPORT

#include <string.h>
#include "plugin_utils.h"
#include "erc721_plugin.h"
#include "eth_plugin_internal.h"
#include "eth_plugin_interface.h"
#include "eth_plugin_handler.h"

static const uint8_t ERC721_APPROVE_SELECTOR[SELECTOR_SIZE] = {0x09, 0x5e, 0xa7, 0xb3};
static const uint8_t ERC721_APPROVE_FOR_ALL_SELECTOR[SELECTOR_SIZE] = {0xa2, 0x2c, 0xb4, 0x65};
static const uint8_t ERC721_TRANSFER_SELECTOR[SELECTOR_SIZE] = {0x23, 0xb8, 0x72, 0xdd};
static const uint8_t ERC721_SAFE_TRANSFER_SELECTOR[SELECTOR_SIZE] = {0x42, 0x84, 0x2e, 0x0e};
static const uint8_t ERC721_SAFE_TRANSFER_DATA_SELECTOR[SELECTOR_SIZE] = {0xb8, 0x8d, 0x4f, 0xde};

const uint8_t *const ERC721_SELECTORS[SELECTORS_COUNT] = {
    ERC721_APPROVE_SELECTOR,
    ERC721_APPROVE_FOR_ALL_SELECTOR,
    ERC721_TRANSFER_SELECTOR,
    ERC721_SAFE_TRANSFER_SELECTOR,
    ERC721_SAFE_TRANSFER_DATA_SELECTOR,
};

void handle_init_contract_721(ethPluginInitContract_t *msg) {
    erc721_context_t *context = (erc721_context_t *) msg->pluginContext;

    if (NO_NFT_METADATA) {
        PRINTF("No NFT metadata when trying to sign!\n");
        msg->result = ETH_PLUGIN_RESULT_ERROR;
        return;
    }
    uint8_t i;
    for (i = 0; i < SELECTORS_COUNT; i++) {
        if (memcmp(PIC(ERC721_SELECTORS[i]), msg->selector, SELECTOR_SIZE) == 0) {
            context->selectorIndex = i;
            break;
        }
    }

    // No selector found.
    if (i == SELECTORS_COUNT) {
        PRINTF("Unknown erc721 selector %.*H\n", SELECTOR_SIZE, msg->selector);
        msg->result = ETH_PLUGIN_RESULT_FALLBACK;
        return;
    }

    msg->result = ETH_PLUGIN_RESULT_OK;
    switch (context->selectorIndex) {
        case SET_APPROVAL_FOR_ALL:
        case APPROVE:
            context->next_param = OPERATOR;
            break;
        case SAFE_TRANSFER:
        case SAFE_TRANSFER_DATA:
        case TRANSFER:
            context->next_param = FROM;
            break;
        default:
            PRINTF("Unsupported selector index: %d\n", context->selectorIndex);
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

void handle_finalize_721(ethPluginFinalize_t *msg) {
    erc721_context_t *context = (erc721_context_t *) msg->pluginContext;

    msg->tokenLookup1 = msg->pluginSharedRO->txContent->destination;
    msg->tokenLookup2 = NULL;
    switch (context->selectorIndex) {
        case TRANSFER:
        case SAFE_TRANSFER:
        case SAFE_TRANSFER_DATA:
        case APPROVE:
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
        // Set Approval for All is not payable
        if (context->selectorIndex == SET_APPROVAL_FOR_ALL) {
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            return;
        } else {
            // Add an additional screen
            msg->numScreens++;
        }
    }
    msg->uiType = ETH_UI_TYPE_GENERIC;
    msg->result = ETH_PLUGIN_RESULT_OK;
}

void handle_provide_info_721(ethPluginProvideInfo_t *msg) {
    msg->result = ETH_PLUGIN_RESULT_OK;
}

void handle_query_contract_id_721(ethQueryContractID_t *msg) {
    erc721_context_t *context = (erc721_context_t *) msg->pluginContext;

    msg->result = ETH_PLUGIN_RESULT_OK;

    strlcpy(msg->name, "NFT", msg->nameLength);

    switch (context->selectorIndex) {
        case SET_APPROVAL_FOR_ALL:
        case APPROVE:
#ifdef HAVE_NBGL
            strlcpy(msg->version, "manage", msg->versionLength);
            strlcat(msg->name, " allowance", msg->nameLength);
#else
            strlcpy(msg->version, "Allowance", msg->versionLength);
#endif
            break;
        case SAFE_TRANSFER:
        case SAFE_TRANSFER_DATA:
        case TRANSFER:
            strlcpy(msg->version, "Transfer", msg->versionLength);
            break;
        default:
            PRINTF("Unsupported selector %d\n", context->selectorIndex);
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

void erc721_plugin_call(int message, void *parameters) {
    switch (message) {
        case ETH_PLUGIN_INIT_CONTRACT: {
            handle_init_contract_721((ethPluginInitContract_t *) parameters);
        } break;
        case ETH_PLUGIN_PROVIDE_PARAMETER: {
            handle_provide_parameter_721((ethPluginProvideParameter_t *) parameters);
        } break;
        case ETH_PLUGIN_FINALIZE: {
            handle_finalize_721((ethPluginFinalize_t *) parameters);
        } break;
        case ETH_PLUGIN_PROVIDE_INFO: {
            handle_provide_info_721((ethPluginProvideInfo_t *) parameters);
        } break;
        case ETH_PLUGIN_QUERY_CONTRACT_ID: {
            handle_query_contract_id_721((ethQueryContractID_t *) parameters);
        } break;
        case ETH_PLUGIN_QUERY_CONTRACT_UI: {
            handle_query_contract_ui_721((ethQueryContractUI_t *) parameters);
        } break;
        default:
            PRINTF("Unhandled message %d\n", message);
            break;
    }
}

#endif  // HAVE_NFT_SUPPORT
