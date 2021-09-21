#include "erc721_plugin.h"

static void set_approval_ui(ethQueryContractUI_t *msg, erc721_parameters_t *context) {
    switch (msg->screenIndex) {
        case 0:
            strlcpy(msg->title, "Allow", msg->titleLength);
            getEthDisplayableAddress(context->address,
                                     msg->msg,
                                     msg->msgLength,
                                     &global_sha3,
                                     chainConfig->chainId);
            break;
        case 1:
            strlcpy(msg->title, "To Spend", msg->titleLength);
            strlcpy(msg->msg, context->collection_name, msg->msgLength);
            break;
        case 2:
            strlcpy(msg->title, "ID", msg->titleLength);
            uint256_to_decimal(context->tokenId,
                               sizeof(context->tokenId),
                               msg->msg,
                               msg->msgLength);
            break;
        default:
            PRINTF("Unsupported screen index %d\n", msg->screenIndex);
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

static void set_approval_for_all_ui(ethQueryContractUI_t *msg, erc721_parameters_t *context) {
    switch (msg->screenIndex) {
        case 0:
            strlcpy(msg->title, "Allow", msg->titleLength);
            getEthDisplayableAddress(context->address,
                                     msg->msg,
                                     msg->msgLength,
                                     &global_sha3,
                                     chainConfig->chainId);
            break;
        case 1:
            strlcpy(msg->title, "To Manage ALL", msg->titleLength);
            strlcpy(msg->msg, context->collection_name, msg->msgLength);
            break;
        default:
            PRINTF("Unsupported screen index %d\n", msg->screenIndex);
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

static void set_transfer_ui(ethQueryContractUI_t *msg, erc721_parameters_t *context) {
    switch (msg->screenIndex) {
        case 0:
            strlcpy(msg->title, "To", msg->titleLength);
            getEthDisplayableAddress(context->address,
                                     msg->msg,
                                     msg->msgLength,
                                     &global_sha3,
                                     chainConfig->chainId);
            break;
        case 1:
            strlcpy(msg->title, "ID", msg->titleLength);
            uint256_to_decimal(context->tokenId,
                               sizeof(context->tokenId),
                               msg->msg,
                               msg->msgLength);
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
            set_approval_ui(msg, context);
            break;
        case SET_APPROVAL_FOR_ALL:
            set_approval_for_all_ui(msg, context);
            break;
        case SAFE_TRANSFER_DATA:
        case SAFE_TRANSFER:
        case TRANSFER:
            set_transfer_ui(msg, context);
            break;
        default:
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            PRINTF("Unsupported selector index %d\n", context->selectorIndex);
            break;
    }
}