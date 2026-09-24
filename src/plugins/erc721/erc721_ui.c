#include <string.h>
#include "erc721_plugin.h"
#include "erc721_internal.h"
#include "eth_plugin_internal.h"
#include "eth_plugin_interface.h"
#include "common_utils.h"
#include "network.h"
#include "shared_context.h"

static void set_approval_ui(ethQueryContractUI_t *msg, erc721_context_t *context) {
    switch (msg->screenIndex) {
        case 0:
            strlcpy(msg->title, "Allow", msg->titleLength);
            if (!getEthDisplayableAddress(context->address,
                                          msg->msg,
                                          msg->msgLength,
                                          g_chain_config->chain_id)) {
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
                                          g_chain_config->chain_id)) {
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
                                          g_chain_config->chain_id)) {
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
                                          g_chain_config->chain_id)) {
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
            strlcpy(msg->title, "NFT Owner", msg->titleLength);
            if (!getEthDisplayableAddress(context->ownerAddress,
                                          msg->msg,
                                          msg->msgLength,
                                          g_chain_config->chain_id)) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
            }
            break;
        case 1:
            strlcpy(msg->title, "To", msg->titleLength);
            if (!getEthDisplayableAddress(context->address,
                                          msg->msg,
                                          msg->msgLength,
                                          g_chain_config->chain_id)) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
            }
            break;
        case 2:
            strlcpy(msg->title, "Collection Name", msg->titleLength);
            strlcpy(msg->msg, msg->item1->nft.collectionName, msg->msgLength);
            break;
        case 3:
            strlcpy(msg->title, "NFT Address", msg->titleLength);
            if (!getEthDisplayableAddress(msg->item1->nft.contractAddress,
                                          msg->msg,
                                          msg->msgLength,
                                          g_chain_config->chain_id)) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
            }
            break;
        case 4:
            strlcpy(msg->title, "NFT ID", msg->titleLength);
            if (!uint256_to_decimal(context->tokenId,
                                    sizeof(context->tokenId),
                                    msg->msg,
                                    msg->msgLength)) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
            }
            break;
        case 5:
            // Requested by finalize only for a non-empty payload
            strlcpy(msg->title, "Transfer With Data", msg->titleLength);
            snprintf(msg->msg, msg->msgLength, "%u bytes", (unsigned) context->data_length);
            break;
        default:
            PRINTF("Unsupported screen index %d\n", msg->screenIndex);
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

static void set_amount_ui(ethQueryContractUI_t *msg) {
    uint64_t chain_id;
    const char *ticker;

    if (msg->txContent == NULL) {
        msg->result = ETH_PLUGIN_RESULT_ERROR;
        return;
    }
    chain_id = get_tx_chain_id();
    ticker = get_displayable_ticker(&chain_id, g_chain_config, true);

    strlcpy(msg->title, "Amount", msg->titleLength);
    if (!amountToString(msg->txContent->value.value,
                        msg->txContent->value.length,
                        WEI_TO_ETHER,
                        ticker,
                        msg->msg,
                        msg->msgLength)) {
        msg->result = ETH_PLUGIN_RESULT_ERROR;
    }
}

void handle_query_contract_ui_721(ethQueryContractUI_t *msg) {
    erc721_context_t *context = (erc721_context_t *) msg->pluginContext;

    if (msg->item1 == NULL) {
        msg->result = ETH_PLUGIN_RESULT_ERROR;
        return;
    }
    msg->result = ETH_PLUGIN_RESULT_OK;
    // Claimed before the per-selector rows: finalize placed it past their last index
    if ((context->amount_screen != 0) && (msg->screenIndex == context->amount_screen)) {
        set_amount_ui(msg);
        return;
    }
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
