#include "erc721_plugin.h"
#include "erc721_internal.h"
#include "plugin_utils.h"
#include "eth_plugin_internal.h"
#include "common_utils.h"

// ABI words are 32 bytes; reject values that don't fit a u32 instead of
// silently truncating them
static bool get_word_u32(const uint8_t *parameter, uint32_t *out) {
    if (allzeroes(parameter, PARAMETER_LENGTH - sizeof(*out)) != 1) {
        return false;
    }
    *out = U4BE(parameter, PARAMETER_LENGTH - sizeof(*out));
    return true;
}

// Read a dynamic offset and rebase it past the selector; rejects overflow
static bool get_dynamic_offset(const uint8_t *parameter, uint32_t *out) {
    uint32_t offset;

    if (!get_word_u32(parameter, &offset) || (offset > (UINT32_MAX - SELECTOR_SIZE))) {
        return false;
    }
    *out = offset + SELECTOR_SIZE;
    return true;
}

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
            // Retain the owner, the account actually debited
            copy_address(context->ownerAddress, msg->parameter, sizeof(context->ownerAddress));
            context->next_param = TO;
            break;
        case TO:
            copy_address(context->address, msg->parameter, sizeof(context->address));
            context->next_param = TOKEN_ID;
            break;
        case TOKEN_ID:
            copy_parameter(context->tokenId, msg->parameter, sizeof(context->tokenId));
            // the bytes-carrying overload has a payload after the token id
            context->next_param =
                (context->selectorIndex == SAFE_TRANSFER_DATA) ? DATA_OFFSET : NONE;
            break;
        case DATA_OFFSET:
            if (!get_dynamic_offset(msg->parameter, &context->data_offset)) {
                PRINTF("Receiver data offset out of range!\n");
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                break;
            }
            context->next_param = DATA_LENGTH;
            break;
        case DATA_LENGTH:
            if (msg->parameterOffset < context->data_offset) {
                // not there yet
                break;
            }
            if ((msg->parameterOffset != context->data_offset) ||
                !get_word_u32(msg->parameter, &context->data_length)) {
                PRINTF("Receiver data length not where it was declared!\n");
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                break;
            }
            // The payload words themselves follow and are tolerated by the
            // non-strict default below; the length is what the user is shown.
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
