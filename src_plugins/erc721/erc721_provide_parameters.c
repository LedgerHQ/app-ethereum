#include "erc721_plugin.h"

static void copy_address(uint8_t *dst, uint8_t *parameter, uint8_t dst_size) {
    uint8_t copy_size = MIN(dst_size, ADDRESS_LENGTH);
    memmove(dst, parameter + PARAMETER_LENGTH - copy_size, copy_size);
}

static void copy_parameter(uint8_t *dst, uint8_t *parameter, uint8_t dst_size) {
    uint8_t copy_size = MIN(dst_size, PARAMETER_LENGTH);
    memmove(dst, parameter, copy_size);
}

void handle_approve(ethPluginProvideParameter_t *msg, erc721_parameters_t *context) {
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

// SCOTT: todo: add display of ETH if there's any?
// `strict` will set msg->result to ERROR if parsing continues after `TOKEN_ID` has been parsed.
void handle_transfer(ethPluginProvideParameter_t *msg, erc721_parameters_t *context, bool strict) {
    switch (context->next_param) {
        case FROM:
            context->next_param = TO;
            break;
        case TO:
            copy_address(context->address, msg->parameter, sizeof(context->address));
            context->next_param = TOKEN_ID;
            break;
        case TOKEN_ID:
            copy_address(context->tokenId, msg->parameter, sizeof(context->tokenId));
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

void handle_approval_for_all(ethPluginProvideParameter_t *msg, erc721_parameters_t *context) {
    switch (context->next_param) {
        case OPERATOR:
            context->next_param = APPROVED;
            break;
        case APPROVED:
            context->next_param = NONE;
            break;
        default:
            PRINTF("Param %d not supported\n", context->next_param);
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

void handle_provide_parameter(void *parameters) {
    ethPluginProvideParameter_t *msg = (ethPluginProvideParameter_t *) parameters;
    erc721_parameters_t *context = (erc721_parameters_t *) msg->pluginContext;

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