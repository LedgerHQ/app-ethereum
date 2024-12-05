#ifdef HAVE_NFT_SUPPORT

#include <string.h>
#include "erc721_plugin.h"
#include "eth_plugin_internal.h"
#include "eth_plugin_interface.h"
#include "common_utils.h"

static void set_approval_ui(ethQueryContractUI_t *msg, erc721_context_t *context) {
    switch (msg->screenIndex) {
        case 0:
            strlcpy(msg->title, "Allow", msg->titleLength);
            if (!getEthDisplayableAddress(context->address,
                                          msg->msg,
                                          msg->msgLength,
                                          chainConfig->chainId)) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
            }
            break;
        case 1:
            strlcpy(msg->title, "To Manage Your", msg->titleLength);
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
        default:
            PRINTF("Unsupported screen index %d\n", msg->screenIndex);
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

static void set_approval_for_all_ui(ethQueryContractUI_t *msg, erc721_context_t *context) {
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

static void set_transfer_ui(ethQueryContractUI_t *msg, erc721_context_t *context) {
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
        default:
            PRINTF("Unsupported screen index %d\n", msg->screenIndex);
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

void handle_query_contract_ui_721(ethQueryContractUI_t *msg) {
    erc721_context_t *context = (erc721_context_t *) msg->pluginContext;

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

#endif  // HAVE_NFT_SUPPORT
