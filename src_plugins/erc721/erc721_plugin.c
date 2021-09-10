#include "erc721_plugin.h"

static void handle_init_contract(void *parameters) {
    ethPluginInitContract_t *msg = (ethPluginInitContract_t *) parameters;
    erc721_parameters_t *context = (erc721_parameters_t *) msg->pluginContext;

    // Sanity checks
    if (!allzeroes(msg->pluginSharedRO->txContent->value.value, 32)) {
        PRINTF("Err: Transaction amount is not 0 for erc721 approval\n");
        msg->result = ETH_PLUGIN_RESULT_ERROR;
    } else if (msg->dataSize != APPROVAL_DATA_SIZE) {
        PRINTF("Invalid erc721 approval data size %d\n", msg->dataSize);
        msg->result = ETH_PLUGIN_RESULT_ERROR;
    }

    uint8_t i;
    for (i = 0; i < NUM_ERC721_SELECTORS; i++) {
        if (memcmp((uint8_t *) PIC(ERC721_SELECTORS[i]), msg->selector, SELECTOR_SIZE) == 0) {
            context->selectorIndex = i;
            break;
        }
    }

    // No selector found.
    if (i == NUM_ERC721_SELECTORS) {
        PRINTF("Unknown erc721 selector %.*H\n", SELECTOR_SIZE, msg->selector);
        msg->result = ETH_PLUGIN_RESULT_ERROR;
        return;
    }

    msg->result = ETH_PLUGIN_RESULT_OK;
    switch (context->selectorIndex) {
        case APPROVAL:
            context->next_param = FIRST;
            break;
        default:
            PRINTF("Unsupported selector index: %d\n", context->selectorIndex);
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

static void handle_provide_parameter(void *parameters) {
    ethPluginProvideParameter_t *msg = (ethPluginProvideParameter_t *) parameters;
    erc721_parameters_t *context = (erc721_parameters_t *) msg->pluginContext;

    PRINTF("erc721 plugin provide parameter %d %.*H\n",
           msg->parameterOffset,
           PARAMETER_LENGTH,
           msg->parameter);

    msg->result = ETH_PLUGIN_RESULT_SUCCESSFUL;
    switch (msg->parameterOffset) {
        case SELECTOR_SIZE:
            memmove(context->address,
                    msg->parameter + PARAMETER_LENGTH - ADDRESS_LENGTH,
                    ADDRESS_LENGTH);
            break;
        case SELECTOR_SIZE + PARAMETER_LENGTH:
            memmove(context->tokenId, msg->parameter, PARAMETER_LENGTH);
            break;
        default:
            PRINTF("Unhandled parameter offset\n");
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

static void handle_finalize(void *parameters) {
    ethPluginFinalize_t *msg = (ethPluginFinalize_t *) parameters;
    erc721_parameters_t *context = (erc721_parameters_t *) msg->pluginContext;

    msg->tokenLookup1 = msg->pluginSharedRO->txContent->destination;
    msg->tokenLookup2 = context->address;
    msg->numScreens = 3;
    msg->uiType = ETH_UI_TYPE_GENERIC;
    msg->result = ETH_PLUGIN_RESULT_OK;
}

static void handle_provide_token(void *parameters) {
    ethPluginProvideToken_t *msg = (ethPluginProvideToken_t *) parameters;
    erc721_parameters_t *context = (erc721_parameters_t *) msg->pluginContext;

    msg->result = ETH_PLUGIN_RESULT_OK;
}

static void handle_contract_id(void *parameters) {
    ethQueryContractID_t *msg = (ethQueryContractID_t *) parameters;

    strlcpy(msg->name, "NFT", msg->nameLength);
    strlcpy(msg->version, "Allowance", msg->versionLength);
    msg->result = ETH_PLUGIN_RESULT_OK;
}

static void handle_contract_ui(void *parameters) {
    ethQueryContractUI_t *msg = (ethQueryContractUI_t *) parameters;
    erc721_parameters_t *context = (erc721_parameters_t *) msg->pluginContext;

    // Collection Name (fallback to contract addr)
    // ID?

    msg->result = ETH_PLUGIN_RESULT_OK;
    switch (context->selectorIndex) {
        case APPROVAL:
            switch (msg->screenIndex) {
                case 0:
                    strlcpy(msg->title, "Collection", msg->titleLength);
                    getEthDisplayableAddress(msg->pluginSharedRO->txContent->destination,
                                             msg->pluginSharedRO->txContent->destinationLength,
                                             msg->msgLength,
                                             &global_sha3,
                                             chainConfig->chainId);
                    break;
                case 1:
                    strlcpy(msg->title, "To spend", msg->titleLength);
                    strlcpy(msg->msg, context->collection_name, msg->msgLength);
                    break;
                case 2:
                    strlcpy(msg->title, "TokenID", msg->titleLength);
                    snprintf(msg->msg, 70, "0x%.*H", sizeof(context->tokenId), context->tokenId);
                    break;
                default:
                    PRINTF("Unsupported screen index %d\n", msg->screenIndex);
                    msg->result = ETH_PLUGIN_RESULT_ERROR;
                    break;
            }
            break;
        default:
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            PRINTF("Unsupported selector index %d\n", context->selectorIndex);
            break;
    }
}

void erc721_plugin_call(int message, void *parameters) {
    switch (message) {
        case ETH_PLUGIN_INIT_CONTRACT: {
            handle_init_contract(parameters);
        } break;
        case ETH_PLUGIN_PROVIDE_PARAMETER: {
            handle_provide_parameter(parameters);
        } break;
        case ETH_PLUGIN_FINALIZE: {
            handle_finalize(parameters);
        } break;
        case ETH_PLUGIN_PROVIDE_TOKEN: {
            handle_provide_token(parameters);
        } break;
        case ETH_PLUGIN_QUERY_CONTRACT_ID: {
            handle_query_contract_id(parameters);
        } break;
        case ETH_PLUGIN_QUERY_CONTRACT_UI: {
            handle_query_contract_ui(parameters);
        } break;
        default:
            PRINTF("Unhandled message %d\n", message);
    }
}
