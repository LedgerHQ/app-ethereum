#include <string.h>
#include "erc1155_plugin.h"
#include "erc1155_internal.h"
#include "plugin_utils.h"
#include "eth_plugin_internal.h"
#include "common_utils.h"

// ABI array lengths are 32-byte words; reject values that don't fit a u16
// instead of silently truncating them
static bool get_word_u16(const uint8_t *parameter, uint16_t *out) {
    if (allzeroes(parameter, PARAMETER_LENGTH - sizeof(*out)) != 1) {
        return false;
    }
    *out = U2BE(parameter, PARAMETER_LENGTH - sizeof(*out));
    return true;
}

// ABI dynamic offsets are 32-byte words; reject values that don't fit a u32
// instead of silently truncating them
static bool get_word_u32(const uint8_t *parameter, uint32_t *out) {
    if (allzeroes(parameter, PARAMETER_LENGTH - sizeof(*out)) != 1) {
        return false;
    }
    *out = U4BE(parameter, PARAMETER_LENGTH - sizeof(*out));
    return true;
}

// Read a dynamic-array offset and rebase it past the selector; rejects overflow
static bool get_dynamic_offset(const uint8_t *parameter, uint32_t *out) {
    uint32_t offset;

    if (!get_word_u32(parameter, &offset) || (offset > (UINT32_MAX - SELECTOR_SIZE))) {
        return false;
    }
    *out = offset + SELECTOR_SIZE;
    return true;
}

static void handle_data_offset(ethPluginProvideParameter_t *msg, erc1155_context_t *context) {
    if (!get_dynamic_offset(msg->parameter, &context->data_offset)) {
        PRINTF("Receiver data offset out of range!\n");
        msg->result = ETH_PLUGIN_RESULT_ERROR;
    }
}

// Returns true once the payload length word has been read
static bool handle_data_length(ethPluginProvideParameter_t *msg, erc1155_context_t *context) {
    if (msg->parameterOffset < context->data_offset) {
        // not there yet
        return false;
    }
    if ((msg->parameterOffset != context->data_offset) ||
        !get_word_u32(msg->parameter, &context->data_length)) {
        PRINTF("Receiver data length not where it was declared!\n");
        msg->result = ETH_PLUGIN_RESULT_ERROR;
        return false;
    }
    // The payload words themselves follow and are tolerated by the non-strict
    // default case; the length is what the user is shown.
    return true;
}

static void handle_safe_transfer(ethPluginProvideParameter_t *msg, erc1155_context_t *context) {
    uint8_t new_value[INT256_LENGTH];

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
            context->next_param = VALUE;
            break;
        case VALUE:
            copy_parameter(new_value, msg->parameter, sizeof(new_value));
            if (!convertUint256BE(new_value, INT256_LENGTH, &context->value)) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                return;
            }
            context->next_param = DATA_OFFSET;
            break;
        case DATA_OFFSET:
            handle_data_offset(msg, context);
            context->next_param = DATA_LENGTH;
            break;
        case DATA_LENGTH:
            if (handle_data_length(msg, context)) {
                context->next_param = NONE;
            }
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
            // Retain the owner, the account actually debited
            copy_address(context->ownerAddress, msg->parameter, sizeof(context->ownerAddress));
            context->next_param = TO;
            break;
        case TO:
            copy_address(context->address, msg->parameter, sizeof(context->address));
            context->next_param = TOKEN_IDS_OFFSET;
            break;
        case TOKEN_IDS_OFFSET:
            if (!get_dynamic_offset(msg->parameter, &context->ids_offset)) {
                PRINTF("Token ids offset out of range!\n");
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                break;
            }
            context->next_param = VALUE_OFFSET;
            break;
        case VALUE_OFFSET:
            if (!get_dynamic_offset(msg->parameter, &context->values_offset)) {
                PRINTF("Values offset out of range!\n");
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                break;
            }
            context->next_param = DATA_OFFSET;
            break;
        case DATA_OFFSET:
            handle_data_offset(msg, context);
            context->next_param = TOKEN_IDS_LENGTH;
            break;
        case TOKEN_IDS_LENGTH:
            if (msg->parameterOffset < context->ids_offset) {
                // not there yet
                break;
            }
            if (msg->parameterOffset != context->ids_offset) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                break;
            }
            if (!get_word_u16(msg->parameter, &context->ids_array_len)) {
                PRINTF("Token ids array too long!\n");
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                break;
            }
            context->batch_displayed = (context->ids_array_len > ERC1155_BATCH_DISPLAY_MAX)
                                           ? ERC1155_BATCH_DISPLAY_MAX
                                           : (uint8_t) context->ids_array_len;
            context->batch_truncated = context->ids_array_len > ERC1155_BATCH_DISPLAY_MAX;
            context->next_param = (context->ids_array_len == 0) ? VALUE_LENGTH : TOKEN_ID;
            // set to zero for next step
            context->array_index = 0;
            break;
        case TOKEN_ID:
            // Surface the first batch entries to the user so a malicious
            // batch cannot hide a high-value token ID behind innocuous ones.
            if (context->array_index < ERC1155_BATCH_DISPLAY_MAX) {
                memcpy(context->batch_ids[context->array_index], msg->parameter, INT256_LENGTH);
            }
            if (--context->ids_array_len == 0) {
                context->next_param = VALUE_LENGTH;
            }
            context->array_index++;
            break;
        case VALUE_LENGTH:
            if (msg->parameterOffset < context->values_offset) {
                // not there yet
                break;
            }
            if (msg->parameterOffset != context->values_offset) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                break;
            }
            if (!get_word_u16(msg->parameter, &context->values_array_len)) {
                PRINTF("Values array too long!\n");
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                break;
            }
            if (context->values_array_len != context->array_index) {
                // ERC-1155 requires the two arrays to be the same length
                PRINTF("Token ids and values array sizes mismatch!\n");
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                break;
            }
            // An empty batch has nothing to consume in VALUE
            context->next_param = (context->values_array_len == 0) ? DATA_LENGTH : VALUE;
            // set to zero for next step
            context->array_index = 0;
            explicit_bzero(&context->value, sizeof(context->value));
            break;
        case VALUE:
            // put it temporarily in token id since we don't use it in batch transfer
            copy_parameter(context->tokenId, msg->parameter, sizeof(context->value));
            // Keep the first per-id values so they pair up with batch_ids
            // entries on screen. Anything past ERC1155_BATCH_DISPLAY_MAX is
            // still aggregated into the total below.
            if (context->array_index < ERC1155_BATCH_DISPLAY_MAX) {
                memcpy(context->batch_values[context->array_index], msg->parameter, INT256_LENGTH);
            }
            if (!convertUint256BE(context->tokenId, sizeof(context->tokenId), &new_value)) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                return;
            }
            add256(&context->value, &new_value, &context->value);
            // Reject crafted batches whose per-id totals wrap uint256. With
            // the partial sum already stored in context->value, an overflow
            // would silently misreport the aggregate "total quantity" shown
            // to the user.
            if (gt256(&new_value, &context->value)) {
                PRINTF("Batch transfer aggregate quantity overflow\n");
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                break;
            }
            if (--context->values_array_len == 0) {
                context->next_param = DATA_LENGTH;
            }
            context->array_index++;
            break;
        case DATA_LENGTH:
            if (handle_data_length(msg, context)) {
                context->next_param = NONE;
            }
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
