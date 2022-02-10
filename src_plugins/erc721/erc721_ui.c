#ifdef HAVE_NFT_SUPPORT

#include "erc721_plugin.h"

static void set_approval_ui(ethQueryContractUI_t *msg, erc721_context_t *context) {
    uint8_t *nft_addr;

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
            strlcpy(msg->title, "To Manage Your", msg->titleLength);
            if (msg->item1) {
                strlcpy(msg->msg, (const char *) &msg->item1->nft.collectionName, msg->msgLength);
            } else {
                strlcpy(msg->msg, "Not Found", msg->msgLength);
            }
            break;
        case 2:
            strlcpy(msg->title, "NFT Address", msg->titleLength);
            // In case of no PROVIDE_NFT_INFO, we already have the address from the SET_PLUGIN
            nft_addr = ((msg->item1) ? ((uint8_t *) msg->item1->nft.contractAddress)
                                     : msg->pluginSharedRO->txContent->destination);
            getEthDisplayableAddress(nft_addr,
                                     msg->msg,
                                     msg->msgLength,
                                     &global_sha3,
                                     chainConfig->chainId);
            break;
        case 3:
            strlcpy(msg->title, "NFT ID", msg->titleLength);
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

static void set_approval_for_all_ui(ethQueryContractUI_t *msg, erc721_context_t *context) {
    uint8_t *nft_addr;

    switch (msg->screenIndex) {
        case 0:
            if (context->approved) {
                strlcpy(msg->title, "Allow", msg->titleLength);
            } else {
                strlcpy(msg->title, "Revoke", msg->titleLength);
            }
            getEthDisplayableAddress(context->address,
                                     msg->msg,
                                     msg->msgLength,
                                     &global_sha3,
                                     chainConfig->chainId);
            break;
        case 1:
            strlcpy(msg->title, "To Manage ALL", msg->titleLength);
            if (msg->item1) {
                strlcpy(msg->msg, (const char *) &msg->item1->nft.collectionName, msg->msgLength);
            } else {
                strlcpy(msg->msg, "Not found", msg->msgLength);
            }
            break;
        case 2:
            strlcpy(msg->title, "NFT Address", msg->titleLength);
            // In case of no PROVIDE_NFT_INFO, we already have the address from the SET_PLUGIN
            nft_addr = ((msg->item1) ? ((uint8_t *) msg->item1->nft.contractAddress)
                                     : msg->pluginSharedRO->txContent->destination);
            getEthDisplayableAddress(nft_addr,
                                     msg->msg,
                                     msg->msgLength,
                                     &global_sha3,
                                     chainConfig->chainId);
            break;
        default:
            PRINTF("Unsupported screen index %d\n", msg->screenIndex);
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

static void set_transfer_ui(ethQueryContractUI_t *msg, erc721_context_t *context) {
    uint8_t *nft_addr;

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
            strlcpy(msg->title, "Collection Name", msg->titleLength);
            if (msg->item1) {
                strlcpy(msg->msg, (const char *) &msg->item1->nft.collectionName, msg->msgLength);
            } else {
                strlcpy(msg->msg, "Not Found", msg->msgLength);
            }
            break;
        case 2:
            strlcpy(msg->title, "NFT Address", msg->titleLength);
            // In case of no PROVIDE_NFT_INFO, we already have the address from the SET_PLUGIN
            nft_addr = ((msg->item1) ? ((uint8_t *) msg->item1->nft.contractAddress)
                                     : msg->pluginSharedRO->txContent->destination);
            getEthDisplayableAddress(nft_addr,
                                     msg->msg,
                                     msg->msgLength,
                                     &global_sha3,
                                     chainConfig->chainId);
            break;
        case 3:
            strlcpy(msg->title, "NFT ID", msg->titleLength);
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

void handle_query_contract_ui_721(void *parameters) {
    ethQueryContractUI_t *msg = (ethQueryContractUI_t *) parameters;
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
