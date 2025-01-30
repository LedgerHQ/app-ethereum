#include <string.h>
#include "eth_plugin_internal.h"
#include "eth_plugin_handler.h"
#include "shared_context.h"
#include "plugin_utils.h"
#include "common_utils.h"

typedef enum { ERC20_TRANSFER = 0, ERC20_APPROVE } erc20Selector_t;

#define MAX_CONTRACT_NAME_LEN 15

typedef struct erc20_parameters_t {
    uint8_t selectorIndex;
    uint8_t destinationAddress[21];
    uint8_t amount[INT256_LENGTH];
    char ticker[MAX_TICKER_LEN];
    uint8_t decimals;
    char contract_name[MAX_CONTRACT_NAME_LEN];
} erc20_parameters_t;

void erc20_plugin_call(int message, void *parameters) {
    switch (message) {
        case ETH_PLUGIN_INIT_CONTRACT: {
            ethPluginInitContract_t *msg = (ethPluginInitContract_t *) parameters;
            erc20_parameters_t *context = (erc20_parameters_t *) msg->pluginContext;
            // enforce that ETH amount should be 0
            if (!allzeroes(msg->pluginSharedRO->txContent->value.value, 32)) {
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
                    PRINTF("Unhandled parameter offset\n");
                    msg->result = ETH_PLUGIN_RESULT_ERROR;
                    break;
            }
        } break;

        case ETH_PLUGIN_FINALIZE: {
            ethPluginFinalize_t *msg = (ethPluginFinalize_t *) parameters;
            erc20_parameters_t *context = (erc20_parameters_t *) msg->pluginContext;
            PRINTF("erc20 plugin finalize\n");
            if (context->selectorIndex == ERC20_TRANSFER) {
                msg->tokenLookup1 = msg->pluginSharedRO->txContent->destination;
                msg->amount = context->amount;
                msg->address = context->destinationAddress;
                msg->uiType = ETH_UI_TYPE_AMOUNT_ADDRESS;
                msg->result = ETH_PLUGIN_RESULT_OK;
            } else if (context->selectorIndex == ERC20_APPROVE) {
                msg->tokenLookup1 = msg->pluginSharedRO->txContent->destination;
                msg->numScreens = 2;
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
#ifdef HAVE_NBGL
            strlcpy(msg->name, "ERC20 token", msg->nameLength);
#else
            strlcpy(msg->name, "Type", msg->nameLength);
#endif
            strlcpy(msg->version, "Approve", msg->versionLength);
            msg->result = ETH_PLUGIN_RESULT_OK;
        } break;

        case ETH_PLUGIN_QUERY_CONTRACT_UI: {
            ethQueryContractUI_t *msg = (ethQueryContractUI_t *) parameters;
            erc20_parameters_t *context = (erc20_parameters_t *) msg->pluginContext;
            switch (msg->screenIndex) {
                case 0:
                    strlcpy(msg->title, "Amount", msg->titleLength);
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
                    strlcpy(msg->title, "Approve to", msg->titleLength);
                    if (!getEthDisplayableAddress(context->destinationAddress,
                                                  msg->msg,
                                                  msg->msgLength,
                                                  chainConfig->chainId)) {
                        msg->result = ETH_PLUGIN_RESULT_ERROR;
                    }
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;
                default:
                    break;
            }
        } break;

        default:
            PRINTF("Unhandled message %d\n", message);
    }
}
