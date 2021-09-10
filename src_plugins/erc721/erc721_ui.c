#include "erc721_plugin.h"

static void handle_approval_ui(ethQueryContractUI_t *msg, erc721_parameters_t *context) {
    switch (msg->screenIndex) {
        case 0:
            strlcpy(msg->title, "Collection", msg->titleLength);
            getEthDisplayableAddress(msg->pluginSharedRO->txContent->destination,
                                     msg->msg,
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
}

void handle_query_contract_ui(void *parameters) {
    ethQueryContractUI_t *msg = (ethQueryContractUI_t *) parameters;
    erc721_parameters_t *context = (erc721_parameters_t *) msg->pluginContext;

    msg->result = ETH_PLUGIN_RESULT_OK;
    switch (context->selectorIndex) {
        case APPROVE:
            handle_approval_ui(msg, context);
            break;
        default:
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            PRINTF("Unsupported selector index %d\n", context->selectorIndex);
            break;
    }
}