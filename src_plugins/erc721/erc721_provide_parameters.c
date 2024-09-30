#ifdef HAVE_NFT_SUPPORT

#include "erc721_plugin.h"
#include "plugin_utils.h"
#include "eth_plugin_internal.h"

static void handle_approve(ethPluginProvideParameter_t *msg, erc721_context_t *context) {
    switch (context->next_param) {
        case OPERATOR:
            copy_address(context->address, msg->parameter, sizeof(context->address));
            context->next_param = TOKEN_ID;
            break;
        case TOKEN_ID:
            copy_parameter(context->tokenId, msg->parameter, sizeof(context->tokenId));
            context->next_param = NONE;
            break;
        default:
            PRINTF("Unhandled parameter offset\n");
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

// `strict` will set msg->result to ERROR if parsing continues after `TOKEN_ID` has been parsed.
static void handle_transfer(ethPluginProvideParameter_t *msg,
                            erc721_context_t *context,
                            bool strict) {
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
            context->next_param = NONE;
            break;
        default:
            if (strict) {
                PRINTF("Param %d not supported\n", context->next_param);
                msg->result = ETH_PLUGIN_RESULT_ERROR;
            }
            break;
    }
}

static void handle_approval_for_all(ethPluginProvideParameter_t *msg, erc721_context_t *context) {
    switch (context->next_param) {
        case OPERATOR:
            context->next_param = APPROVED;
            copy_address(context->address, msg->parameter, sizeof(context->address));
            break;
        case APPROVED:
            context->next_param = NONE;
            context->approved = msg->parameter[PARAMETER_LENGTH - 1];
            break;
        default:
            PRINTF("Param %d not supported\n", context->next_param);
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

void handle_provide_parameter_721(ethPluginProvideParameter_t *msg) {
    erc721_context_t *context = (erc721_context_t *) msg->pluginContext;

    PRINTF("erc721 plugin provide parameter %d %.*H\n",
           msg->parameterOffset,
           PARAMETER_LENGTH,
           msg->parameter);

    msg->result = ETH_PLUGIN_RESULT_SUCCESSFUL;
    switch (context->selectorIndex) {
        case APPROVE:
            handle_approve(msg, context);
            break;
        case SAFE_TRANSFER:
        case TRANSFER:
            handle_transfer(msg, context, true);
            break;
        case SAFE_TRANSFER_DATA:
            // Set `strict` to `false` because additional data might be present.
            handle_transfer(msg, context, false);
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
