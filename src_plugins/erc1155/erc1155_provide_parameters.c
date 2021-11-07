#include "erc1155_plugin.h"
#include "eth_plugin_internal.h"

static void handle_safe_transfer(ethPluginProvideParameter_t *msg, erc1155_context_t *context) {
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
            copy_parameter(context->value, msg->parameter, sizeof(context->value));
            context->next_param = NONE;
            break;
        default:
            // Some extra data might be present so don't error.
            break;
    }
}

static void handle_batch_transfer(ethPluginProvideParameter_t *msg, erc1155_context_t *context) {
    switch (context->next_param) {
        case FROM:
            context->next_param = TO;
            break;
        case TO:
            copy_address(context->address, msg->parameter, sizeof(context->address));
            context->next_param = TOKEN_ID;
            break;
        case TOKEN_IDS_OFFSET:
            context->tokenIdsOffset = U4BE(msg->parameter, PARAMETER_LENGTH - 4);
            context->next_param = VALUE_OFFSET;
            break;
        case VALUE_OFFSET:
            context->targetOffset = context->tokenIdsOffset;
            context->next_param = TOKEN_ID;
            break;
        case TOKEN_ID:
            copy_parameter(context->tokenId, msg->parameter, sizeof(context->tokenId));
            context->targetOffset = context->valueOffset;
            context->next_param = VALUE;
            break;
        case VALUE:
            copy_parameter(context->value, msg->parameter, sizeof(context->value));
            context->targetOffset = 0;
            context->next_param = NONE;
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

void handle_provide_parameter_1155(void *parameters) {
    ethPluginProvideParameter_t *msg = (ethPluginProvideParameter_t *) parameters;
    erc1155_context_t *context = (erc1155_context_t *) msg->pluginContext;

    PRINTF("erc1155 plugin provide parameter %d %.*H\n",
           msg->parameterOffset,
           PARAMETER_LENGTH,
           msg->parameter);

    msg->result = ETH_PLUGIN_RESULT_SUCCESSFUL;

    if (context->targetOffset > SELECTOR_SIZE &&
        context->targetOffset != msg->parameterOffset - SELECTOR_SIZE) {
        return;
    }
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