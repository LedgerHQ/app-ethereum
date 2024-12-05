#ifdef HAVE_NFT_SUPPORT

#include <string.h>
#include "erc1155_plugin.h"
#include "eth_plugin_internal.h"
#include "eth_plugin_interface.h"
#include "common_utils.h"

static void set_approval_for_all_ui(ethQueryContractUI_t *msg, erc1155_context_t *context) {
    switch (msg->screenIndex) {
        case 0:
            if (context->approved) {
                strlcpy(msg->title, "Allow", msg->titleLength);
            } else {
                strlcpy(msg->title, "Revoke", msg->titleLength);
            }
            if (!getEthDisplayableAddress(context->address,
                                          msg->msg,
                                          msg->msgLength,
                                          chainConfig->chainId)) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
            }
            break;
        case 1:
            strlcpy(msg->title, "To Manage ALL", msg->titleLength);
            strlcpy(msg->msg, msg->item1->nft.collectionName, msg->msgLength);
            break;
        case 2:
            strlcpy(msg->title, "NFT Address", msg->titleLength);
            if (!getEthDisplayableAddress(msg->item1->nft.contractAddress,
                                          msg->msg,
                                          msg->msgLength,
                                          chainConfig->chainId)) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
            }
            break;
        default:
            PRINTF("Unsupported screen index %d\n", msg->screenIndex);
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

static void set_transfer_ui(ethQueryContractUI_t *msg, erc1155_context_t *context) {
    switch (msg->screenIndex) {
        case 0:
            strlcpy(msg->title, "To", msg->titleLength);
            if (!getEthDisplayableAddress(context->address,
                                          msg->msg,
                                          msg->msgLength,
                                          chainConfig->chainId)) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
            }
            break;
        case 1:
            strlcpy(msg->title, "Collection Name", msg->titleLength);
            strlcpy(msg->msg, msg->item1->nft.collectionName, msg->msgLength);
            break;
        case 2:
            strlcpy(msg->title, "NFT Address", msg->titleLength);
            if (!getEthDisplayableAddress(msg->item1->nft.contractAddress,
                                          msg->msg,
                                          msg->msgLength,
                                          chainConfig->chainId)) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
            }
            break;
        case 3:
            strlcpy(msg->title, "NFT ID", msg->titleLength);
            if (!uint256_to_decimal(context->tokenId,
                                    sizeof(context->tokenId),
                                    msg->msg,
                                    msg->msgLength)) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
            }
            break;
        case 4:
            strlcpy(msg->title, "Quantity", msg->titleLength);
            if (!tostring256(&context->value, 10, msg->msg, msg->msgLength)) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
            }
            break;
        default:
            PRINTF("Unsupported screen index %d\n", msg->screenIndex);
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

static void set_batch_transfer_ui(ethQueryContractUI_t *msg, erc1155_context_t *context) {
    char quantity_str[48];

    switch (msg->screenIndex) {
        case 0:
            strlcpy(msg->title, "To", msg->titleLength);
            if (!getEthDisplayableAddress(context->address,
                                          msg->msg,
                                          msg->msgLength,
                                          chainConfig->chainId)) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
            }
            break;
        case 1:
            strlcpy(msg->title, "Collection Name", msg->titleLength);
            strlcpy(msg->msg, msg->item1->nft.collectionName, msg->msgLength);
            break;
        case 2:
            strlcpy(msg->title, "NFT Address", msg->titleLength);
            if (!getEthDisplayableAddress(msg->item1->nft.contractAddress,
                                          msg->msg,
                                          msg->msgLength,
                                          chainConfig->chainId)) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
            }
            break;
        case 3:
            strlcpy(msg->title, "Total Quantity", msg->titleLength);
            if (!tostring256(&context->value, 10, &quantity_str[0], sizeof(quantity_str))) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                break;
            }
            snprintf(msg->msg,
                     msg->msgLength,
                     "%s from %d NFT IDs",
                     quantity_str,
                     context->array_index);
            break;
        default:
            PRINTF("Unsupported screen index %d\n", msg->screenIndex);
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

void handle_query_contract_ui_1155(ethQueryContractUI_t *msg) {
    erc1155_context_t *context = (erc1155_context_t *) msg->pluginContext;

    msg->result = ETH_PLUGIN_RESULT_OK;
    switch (context->selectorIndex) {
        case SET_APPROVAL_FOR_ALL:
            set_approval_for_all_ui(msg, context);
            break;
        case SAFE_TRANSFER:
            set_transfer_ui(msg, context);
            break;
        case SAFE_BATCH_TRANSFER:
            set_batch_transfer_ui(msg, context);
            break;
        default:
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            PRINTF("Unsupported selector index %d\n", context->selectorIndex);
            break;
    }
}

#endif  // HAVE_NFT_SUPPORT
