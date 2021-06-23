#include <string.h>
#include "eth_plugin_interface.h"
#include "shared_context.h"
#include "eth_plugin_internal.h"
#include "utils.h"

typedef enum {
    YEARN_WITHDRAW = 0,
    YEARN_DEPOSIT,
    YEARN_WITHDRAW_WITH_AMOUNT,
    YEARN_DEPOSIT_WITH_AMOUNT
} yearnSelector_t;

static const uint8_t YEARN_EXPECTED_DATA_SIZE[] = {
    4,
    4,
    4 + 32,
    4 + 32,
};

typedef struct yearn_parameters_t {
    uint8_t selectorIndex;
    uint8_t amount[32];
    uint8_t token[MAX_TICKER_LEN];
    uint8_t decimals;
} yearn_parameters_t;

void yearn_plugin_call(int message, void *parameters) {
    switch (message) {
        case ETH_PLUGIN_INIT_CONTRACT: {
            ethPluginInitContract_t *msg = (ethPluginInitContract_t *) parameters;
            yearn_parameters_t *context = (yearn_parameters_t *) msg->pluginContext;
            size_t i;

            for (i = 0; i < NUM_YEARN_SELECTORS; i++) {
                if (memcmp((uint8_t *) PIC(YEARN_SELECTORS[i]), msg->selector, SELECTOR_SIZE) ==
                    0) {
                    context->selectorIndex = i;
                    break;
                }
            }

            // Make sure no ETH is direclty transfered.
            if (!allzeroes(msg->pluginSharedRO->txContent->value.value, 32)) {
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                break;
            }

            // Selector out of bound
            if (i == NUM_YEARN_SELECTORS) {
                PRINTF("Unknown selector %.*H\n", SELECTOR_SIZE, msg->selector);
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                break;
            }

            if (msg->dataSize != YEARN_EXPECTED_DATA_SIZE[context->selectorIndex]) {
                PRINTF("Unexpected data size for command %d expected %d got %d\n",
                       context->selectorIndex,
                       YEARN_EXPECTED_DATA_SIZE[context->selectorIndex],
                       msg->dataSize);
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                break;
            }
            PRINTF("yearn plugin inititialized\n");
            msg->result = ETH_PLUGIN_RESULT_OK;
        } break;

        case ETH_PLUGIN_PROVIDE_PARAMETER: {
            ethPluginProvideParameter_t *msg = (ethPluginProvideParameter_t *) parameters;
            yearn_parameters_t *context = (yearn_parameters_t *) msg->pluginContext;

            if (context->selectorIndex == YEARN_WITHDRAW_WITH_AMOUNT ||
                context->selectorIndex == YEARN_DEPOSIT_WITH_AMOUNT) {
                switch (msg->parameterOffset) {
                    case 4:
                        memmove(context->amount, msg->parameter, 32);
                        msg->result = ETH_PLUGIN_RESULT_OK;
                        break;
                    default:
                        PRINTF("Unhandled parameter offset\n");
                        msg->result = ETH_PLUGIN_RESULT_ERROR;
                        break;
                }
            } else {
                PRINTF("Call to contract expects no parameters\n");
                msg->result = ETH_PLUGIN_RESULT_ERROR;
            }
        } break;

        case ETH_PLUGIN_FINALIZE: {
            ethPluginFinalize_t *msg = (ethPluginFinalize_t *) parameters;
            yearn_parameters_t *context = (yearn_parameters_t *) msg->pluginContext;
            PRINTF("yearn plugin finalize\n");
            msg->tokenLookup1 = msg->pluginSharedRO->txContent->destination;
            msg->amount = context->amount;
            msg->uiType = ETH_UI_TYPE_GENERIC;
            msg->result = ETH_PLUGIN_RESULT_OK;
        } break;

        case ETH_PLUGIN_PROVIDE_TOKEN: {
            ethPluginProvideToken_t *msg = (ethPluginProvideToken_t *) parameters;
            yearn_parameters_t *context = (yearn_parameters_t *) msg->pluginContext;
            PRINTF("yearn plugin provide token: %d\n", (msg->token1 != NULL));
            if (msg->token1 != NULL) {
                context->decimals = msg->token1->decimals;
                strcpy((char *) context->token, (char *) msg->token1->ticker);
                msg->result = ETH_PLUGIN_RESULT_OK;
            } else {
                msg->result = ETH_PLUGIN_RESULT_FALLBACK;
            }
        } break;

        case ETH_PLUGIN_QUERY_CONTRACT_ID: {
            ethQueryContractID_t *msg = (ethQueryContractID_t *) parameters;
            yearn_parameters_t *context = (yearn_parameters_t *) msg->pluginContext;
            strcpy(msg->name, "Type");
            switch (context->selectorIndex) {
                case YEARN_WITHDRAW:
                    strcpy(msg->version, "Withdrawal");
                    break;
                case YEARN_WITHDRAW_WITH_AMOUNT:
                    strcpy(msg->version, "Withdrawal");
                    break;

                case YEARN_DEPOSIT:
                    strcpy(msg->version, "Deposit");
                    break;

                case YEARN_DEPOSIT_WITH_AMOUNT:
                    strcpy(msg->version, "Deposit");
                    break;

                default:
                    break;
            }
            msg->result = ETH_PLUGIN_RESULT_OK;
        } break;

        case ETH_PLUGIN_QUERY_CONTRACT_UI: {
            ethQueryContractUI_t *msg = (ethQueryContractUI_t *) parameters;
            yearn_parameters_t *context = (yearn_parameters_t *) msg->pluginContext;
            switch (msg->screenIndex) {
                case 0: {
                    strcpy(msg->title, "Amount");
                    char *ticker_ptr = (char *) context->token;
                    /* skip "yv" in front of cToken unless we use "redeem", as
                    redeem is the only operation dealing with a cToken amount */
                    if (context->selectorIndex == YEARN_DEPOSIT_WITH_AMOUNT) {
                        ticker_ptr++;
                    }
                    amountToString(context->amount,
                                   sizeof(context->amount),
                                   context->decimals,
                                   ticker_ptr,
                                   msg->msg,
                                   100);
                    msg->result = ETH_PLUGIN_RESULT_OK;
                } break;
                case 1:
                    strcpy(msg->title, "Vault");
                    strcat(msg->msg,
                           (char *) context->token +
                               1);  // remove the 'yv' char at beginning of yearn ticker
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
