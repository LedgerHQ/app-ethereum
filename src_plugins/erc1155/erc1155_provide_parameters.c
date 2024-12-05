#ifdef HAVE_NFT_SUPPORT

#include <string.h>
#include "erc1155_plugin.h"
#include "plugin_utils.h"
#include "eth_plugin_internal.h"
#include "common_utils.h"

static void handle_safe_transfer(ethPluginProvideParameter_t *msg, erc1155_context_t *context) {
    uint8_t new_value[INT256_LENGTH];

    switch (context->next_param) {
        case FROM:
            context->next_param = TO;
            break;
        case TO:
            copy_address(context->address, msg->parameter, sizeof(context->address));
            context->next_param = TOKEN_ID;
            break;
        case TOKEN_ID:
            copy_parameter(context->tokenId, msg->parameter, sizeof(context->tokenId));
            context->next_param = VALUE;
            break;
        case VALUE:
            copy_parameter(new_value, msg->parameter, sizeof(new_value));
            convertUint256BE(new_value, INT256_LENGTH, &context->value);
            context->next_param = NONE;
            break;
        default:
            // Some extra data might be present so don't error.
            break;
    }
}

static void handle_batch_transfer(ethPluginProvideParameter_t *msg, erc1155_context_t *context) {
    uint256_t new_value;

    switch (context->next_param) {
        case FROM:
            context->next_param = TO;
            break;
        case TO:
            copy_address(context->address, msg->parameter, sizeof(context->address));
            context->next_param = TOKEN_IDS_OFFSET;
            break;
        case TOKEN_IDS_OFFSET:
            context->ids_offset =
                U4BE(msg->parameter, PARAMETER_LENGTH - sizeof(context->ids_offset)) + 4;
            context->next_param = VALUE_OFFSET;
            break;
        case VALUE_OFFSET:
            context->values_offset =
                U4BE(msg->parameter, PARAMETER_LENGTH - sizeof(context->values_offset)) + 4;
            context->next_param = TOKEN_IDS_LENGTH;
            break;
        case TOKEN_IDS_LENGTH:
            if ((msg->parameterOffset + PARAMETER_LENGTH) > context->ids_offset) {
                context->ids_array_len =
                    U2BE(msg->parameter, PARAMETER_LENGTH - sizeof(context->ids_array_len));
                context->next_param = TOKEN_ID;
                // set to zero for next step
                context->array_index = 0;
            }
            break;
        case TOKEN_ID:
            // don't copy anything since we won't display it
            if (--context->ids_array_len == 0) {
                context->next_param = VALUE_LENGTH;
            }
            context->array_index++;
            break;
        case VALUE_LENGTH:
            if ((msg->parameterOffset + PARAMETER_LENGTH) > context->values_offset) {
                context->values_array_len =
                    U2BE(msg->parameter, PARAMETER_LENGTH - sizeof(context->values_array_len));
                if (context->values_array_len != context->array_index) {
                    PRINTF("Token ids and values array sizes mismatch!");
                }
                context->next_param = VALUE;
                // set to zero for next step
                context->array_index = 0;
                explicit_bzero(&context->value, sizeof(context->value));
            }
            break;
        case VALUE:
            // put it temporarily in token id since we don't use it in batch transfer
            copy_parameter(context->tokenId, msg->parameter, sizeof(context->value));
            convertUint256BE(context->tokenId, sizeof(context->tokenId), &new_value);
            add256(&context->value, &new_value, &context->value);
            if (--context->values_array_len == 0) {
                context->next_param = NONE;
            }
            context->array_index++;
            break;
        default:
            // Some extra data might be present so don't error.
            break;
    }
}

static void handle_approval_for_all(ethPluginProvideParameter_t *msg, erc1155_context_t *context) {
    switch (context->next_param) {
        case OPERATOR:
            context->next_param = APPROVED;
            copy_address(context->address, msg->parameter, sizeof(context->address));
            break;
        case APPROVED:
            context->approved = msg->parameter[PARAMETER_LENGTH - 1];
            context->next_param = NONE;
            break;
        default:
            PRINTF("Param %d not supported\n", context->next_param);
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

void handle_provide_parameter_1155(ethPluginProvideParameter_t *msg) {
    erc1155_context_t *context = (erc1155_context_t *) msg->pluginContext;

    PRINTF("erc1155 plugin provide parameter %d %.*H\n",
           msg->parameterOffset,
           PARAMETER_LENGTH,
           msg->parameter);

    msg->result = ETH_PLUGIN_RESULT_SUCCESSFUL;

    // if (context->targetOffset > SELECTOR_SIZE &&
    //     context->targetOffset != msg->parameterOffset - SELECTOR_SIZE) {
    //     return;
    // }
    switch (context->selectorIndex) {
        case SAFE_TRANSFER:
            handle_safe_transfer(msg, context);
            break;
        case SAFE_BATCH_TRANSFER:
            handle_batch_transfer(msg, context);
            break;
        case SET_APPROVAL_FOR_ALL:
            handle_approval_for_all(msg, context);
            break;
        default:
            PRINTF("Selector index %d not supported\n", context->selectorIndex);
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

#endif  // HAVE_NFT_SUPPORT
