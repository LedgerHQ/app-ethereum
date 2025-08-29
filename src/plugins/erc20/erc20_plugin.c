#include <string.h>
#include "eth_plugin_internal.h"
#include "eth_plugin_handler.h"
#include "shared_context.h"
#include "plugin_utils.h"
#include "common_utils.h"
#include "format.h"
#include "utils.h"

typedef enum { ERC20_TRANSFER = 0, ERC20_APPROVE } erc20Selector_t;

#define MAX_CONTRACT_NAME_LEN 15
#define MAX_EXTRA_DATA_SLOTS  2

typedef struct erc20_parameters_t {
    uint8_t selectorIndex;
    uint8_t destinationAddress[21];
    uint8_t amount[INT256_LENGTH];
    char ticker[MAX_TICKER_LEN];
    uint8_t decimals;
    char contract_name[MAX_CONTRACT_NAME_LEN];
    char extra_data[MAX_EXTRA_DATA_SLOTS * 32 + 1];
    uint8_t extra_data_len;
} erc20_parameters_t;

void erc20_plugin_call(int message, void *parameters) {
    switch (message) {
        case ETH_PLUGIN_INIT_CONTRACT: {
            ethPluginInitContract_t *msg = (ethPluginInitContract_t *) parameters;
            erc20_parameters_t *context = (erc20_parameters_t *) msg->pluginContext;

            memset(context->extra_data, 0, sizeof(context->extra_data));
            context->extra_data_len = 0;

            // enforce that ETH amount should be 0
            if (!allzeroes(msg->txContent->value.value, 32)) {
                PRINTF("Err: Transaction amount is not 0\n");
                msg->result = ETH_PLUGIN_RESULT_ERROR;
            } else {
                size_t i;
                for (i = 0; i < NUM_ERC20_SELECTORS; i++) {
                    if (memcmp((uint8_t *) PIC(ERC20_SELECTORS[i]), msg->selector, SELECTOR_SIZE) ==
                        0) {
                        context->selectorIndex = i;
                        break;
                    }
                }
                if (i == NUM_ERC20_SELECTORS) {
                    PRINTF("Unknown selector %.*H\n", SELECTOR_SIZE, msg->selector);
                    msg->result = ETH_PLUGIN_RESULT_ERROR;
                    break;
                }
                PRINTF("erc20 plugin init\n");
                msg->result = ETH_PLUGIN_RESULT_OK;
            }
        } break;

        case ETH_PLUGIN_PROVIDE_PARAMETER: {
            ethPluginProvideParameter_t *msg = (ethPluginProvideParameter_t *) parameters;
            erc20_parameters_t *context = (erc20_parameters_t *) msg->pluginContext;
            PRINTF("erc20 plugin provide parameter %d %.*H\n",
                   msg->parameterOffset,
                   32,
                   msg->parameter);
            switch (msg->parameterOffset) {
                case 4:
                    memmove(context->destinationAddress, msg->parameter + 12, 20);
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;
                case 4 + 32:
                    memmove(context->amount, msg->parameter, 32);
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;
                default:
                    if (msg->parameterOffset <= 4 + 32 + MAX_EXTRA_DATA_SLOTS * 32) {
                        // store extra data for possible later use
                        size_t extra_data_offset = msg->parameterOffset - (4 + 32 + 32);
                        memmove(context->extra_data + extra_data_offset, msg->parameter, 32);
                        context->extra_data[extra_data_offset + 32] = '\0';
                        context->extra_data_len += 32;
                        msg->result = ETH_PLUGIN_RESULT_OK;
                    } else {
                        PRINTF("Extra data too long to buffer\n");
                        context->extra_data_len = 0;
                        msg->result = ETH_PLUGIN_RESULT_ERROR;
                    }
                    break;
            }
        } break;

        case ETH_PLUGIN_FINALIZE: {
            ethPluginFinalize_t *msg = (ethPluginFinalize_t *) parameters;
            erc20_parameters_t *context = (erc20_parameters_t *) msg->pluginContext;
            PRINTF("erc20 plugin finalize\n");
            if (context->selectorIndex == ERC20_TRANSFER & context->extra_data_len == 0) {
                msg->tokenLookup1 = msg->txContent->destination;
                msg->amount = context->amount;
                msg->address = context->destinationAddress;
                msg->uiType = ETH_UI_TYPE_AMOUNT_ADDRESS;
                msg->result = ETH_PLUGIN_RESULT_OK;
            } else {
                msg->tokenLookup1 = msg->txContent->destination;
                msg->numScreens = (context->extra_data_len == 0 ? 2 : 3);
                msg->uiType = ETH_UI_TYPE_GENERIC;
                msg->result = ETH_PLUGIN_RESULT_OK;
            }
        } break;

        case ETH_PLUGIN_PROVIDE_INFO: {
            ethPluginProvideInfo_t *msg = (ethPluginProvideInfo_t *) parameters;
            erc20_parameters_t *context = (erc20_parameters_t *) msg->pluginContext;
            PRINTF("erc20 plugin provide token 1: %d - 2: %d\n",
                   (msg->item1 != NULL),
                   (msg->item2 != NULL));
            if (msg->item1 != NULL) {
                strlcpy(context->ticker, msg->item1->token.ticker, MAX_TICKER_LEN);
                context->decimals = msg->item1->token.decimals;
                msg->result = ETH_PLUGIN_RESULT_OK;
            } else {
                msg->result = ETH_PLUGIN_RESULT_FALLBACK;
            }
        } break;

        case ETH_PLUGIN_QUERY_CONTRACT_ID: {
            ethQueryContractID_t *msg = (ethQueryContractID_t *) parameters;
            erc20_parameters_t *context = (erc20_parameters_t *) msg->pluginContext;

            strlcpy(msg->name, "ERC20 token", msg->nameLength);

            if (context->selectorIndex == ERC20_TRANSFER) {
                strlcpy(msg->version, "Send", msg->versionLength);
            } else {
                strlcpy(msg->version, "Approve", msg->versionLength);
            }
            msg->result = ETH_PLUGIN_RESULT_OK;
        } break;

        case ETH_PLUGIN_QUERY_CONTRACT_UI: {
            ethQueryContractUI_t *msg = (ethQueryContractUI_t *) parameters;
            erc20_parameters_t *context = (erc20_parameters_t *) msg->pluginContext;
            switch (msg->screenIndex) {
                case 0:
                    if (context->selectorIndex == ERC20_TRANSFER) {
                        strlcpy(msg->title, "Send", msg->titleLength);
                    } else {
                        strlcpy(msg->title, "Approve", msg->titleLength);
                    }
                    if (ismaxint(context->amount, sizeof(context->amount))) {
                        strlcpy(msg->msg, "Unlimited ", msg->msgLength);
                        strlcat(msg->msg, context->ticker, msg->msgLength);
                    } else {
                        if (!amountToString(context->amount,
                                            sizeof(context->amount),
                                            context->decimals,
                                            context->ticker,
                                            msg->msg,
                                            msg->msgLength)) {
                            msg->result = ETH_PLUGIN_RESULT_ERROR;
                            break;
                        }
                    }
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;
                case 1:
                    if (context->selectorIndex == ERC20_TRANSFER) {
                        strlcpy(msg->title, "To", msg->titleLength);
                    } else {
                        strlcpy(msg->title, "Approve to", msg->titleLength);
                    }

                    if (!getEthDisplayableAddress(context->destinationAddress,
                                                  msg->msg,
                                                  msg->msgLength,
                                                  chainConfig->chainId)) {
                        msg->result = ETH_PLUGIN_RESULT_ERROR;
                    }
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;
                case 2: {
                    uint8_t extra_data_len = strnlen(context->extra_data, context->extra_data_len);

                    PRINTF("Extra Data Length %d\n", extra_data_len);
                    PRINTF("Message Length %d\n", msg->msgLength);

                    if (extra_data_len == 0) {
                        // should not happen
                        msg->result = ETH_PLUGIN_RESULT_ERROR;
                        break;
                    }

                    strlcpy(msg->title, "Extra Data", msg->titleLength);

                    if (is_displayable_ascii(context->extra_data, extra_data_len)) {
                        // display as string
                        PRINTF("Display as ASCII string\n");

                        if (extra_data_len >= msg->msgLength) {
                            // truncate
                            extra_data_len = msg->msgLength - 1;
                            context->extra_data[extra_data_len] = '\0';
                        }
                        strlcpy(msg->msg, context->extra_data, msg->msgLength);
                    } else {
                        // display as hex
                        PRINTF("Display as Hex string\n");

                        if ((2 + (extra_data_len * 2) + 1) > msg->msgLength) {
                            // truncate
                            extra_data_len = (msg->msgLength - 2 - 1) / 2;
                        }
                        msg->msg[0] = '0';
                        msg->msg[1] = 'x';
                        if (format_hex((const uint8_t *) context->extra_data,
                                       extra_data_len,
                                       msg->msg + 2,
                                       msg->msgLength - 2) < 0) {
                            msg->result = ETH_PLUGIN_RESULT_ERROR;
                            break;
                        }
                    }
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;
                }

                default:
                    break;
            }
        } break;

        default:
            PRINTF("Unhandled message %d\n", message);
    }
}
