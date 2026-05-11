#include <string.h>
#include "erc1155_plugin.h"
#include "erc1155_internal.h"
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

static void set_transfer_ui(ethQueryContractUI_t *msg, erc1155_context_t *context) {
    switch (msg->screenIndex) {
        case 0:
            strlcpy(msg->title, "To", msg->titleLength);
            if (!getEthDisplayableAddress(context->address,
                                          msg->msg,
                                          msg->msgLength,
                                          g_chain_config->chain_id)) {
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

// Screen indices for the SAFE_BATCH_TRANSFER review. Fixed-index screens
// are mapped 1:1 to the enum; per-pair detail screens (PAIR_BASE..) and the
// truncation warning use dynamic indices computed from batch_displayed.
enum {
    BATCH_SCREEN_TO = 0,
    BATCH_SCREEN_COLLECTION,
    BATCH_SCREEN_NFT_ADDRESS,
    BATCH_SCREEN_TOTAL_QUANTITY,
    BATCH_SCREEN_PAIR_BASE,
};

static void set_batch_transfer_ui(ethQueryContractUI_t *msg, erc1155_context_t *context) {
    char quantity_str[48];
    uint8_t idx = msg->screenIndex;
    uint8_t pair_screens = (uint8_t) (2 * context->batch_displayed);
    uint8_t warn_idx = (uint8_t) (BATCH_SCREEN_PAIR_BASE + pair_screens);

    switch (idx) {
        case BATCH_SCREEN_TO:
            strlcpy(msg->title, "To", msg->titleLength);
            if (!getEthDisplayableAddress(context->address,
                                          msg->msg,
                                          msg->msgLength,
                                          g_chain_config->chain_id)) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
            }
            break;
        case BATCH_SCREEN_COLLECTION:
            strlcpy(msg->title, "Collection Name", msg->titleLength);
            strlcpy(msg->msg, msg->item1->nft.collectionName, msg->msgLength);
            break;
        case BATCH_SCREEN_NFT_ADDRESS:
            strlcpy(msg->title, "NFT Address", msg->titleLength);
            if (!getEthDisplayableAddress(msg->item1->nft.contractAddress,
                                          msg->msg,
                                          msg->msgLength,
                                          g_chain_config->chain_id)) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
            }
            break;
        case BATCH_SCREEN_TOTAL_QUANTITY:
            strlcpy(msg->title, "Total Quantity", msg->titleLength);
            if (!tostring256(&context->value, 10, &quantity_str[0], sizeof(quantity_str))) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                return;
            }
            snprintf(msg->msg,
                     msg->msgLength,
                     "%s from %d NFT IDs",
                     quantity_str,
                     context->array_index);
            break;
        default:
            // Per-pair detail screens, then optional truncation warning.
            if (idx >= BATCH_SCREEN_PAIR_BASE && idx < BATCH_SCREEN_PAIR_BASE + pair_screens) {
                uint8_t pair_offset = (uint8_t) (idx - BATCH_SCREEN_PAIR_BASE);
                uint8_t pair_idx = (uint8_t) (pair_offset / 2);
                bool show_value = (pair_offset % 2) == 1;
                if (show_value) {
                    uint256_t v;
                    convertUint256BE(context->batch_values[pair_idx], INT256_LENGTH, &v);
                    snprintf(msg->title, msg->titleLength, "Quantity #%d", pair_idx + 1);
                    if (!tostring256(&v, 10, msg->msg, msg->msgLength)) {
                        msg->result = ETH_PLUGIN_RESULT_ERROR;
                    }
                } else {
                    snprintf(msg->title, msg->titleLength, "NFT ID #%d", pair_idx + 1);
                    if (!uint256_to_decimal(context->batch_ids[pair_idx],
                                            INT256_LENGTH,
                                            msg->msg,
                                            msg->msgLength)) {
                        msg->result = ETH_PLUGIN_RESULT_ERROR;
                    }
                }
            } else if (context->batch_truncated && idx == warn_idx) {
                strlcpy(msg->title, "WARNING", msg->titleLength);
                snprintf(msg->msg,
                         msg->msgLength,
                         "Only first %d of %d IDs shown",
                         ERC1155_BATCH_DISPLAY_MAX,
                         context->array_index);
            } else {
                PRINTF("Unsupported screen index %d\n", idx);
                msg->result = ETH_PLUGIN_RESULT_ERROR;
            }
            break;
    }
}

void handle_query_contract_ui_1155(ethQueryContractUI_t *msg) {
    erc1155_context_t *context = (erc1155_context_t *) msg->pluginContext;

    if (msg->item1 == NULL) {
        msg->result = ETH_PLUGIN_RESULT_ERROR;
        return;
    }
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
