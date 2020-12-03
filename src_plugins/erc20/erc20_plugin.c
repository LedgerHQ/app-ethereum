#include <string.h>
#include "eth_plugin_internal.h"
#include "eth_plugin_handler.h"
#include "shared_context.h"
#include "ethUtils.h"
#include "utils.h"

typedef enum { ERC20_TRANSFER = 0, ERC20_APPROVE } erc20Selector_t;

typedef enum { TARGET_ADDRESS = 0, TARGET_CONTRACT, TARGET_COMPOUND } targetType_t;

typedef struct erc20_parameters_t {
    uint8_t selectorIndex;
    uint8_t destinationAddress[21];
    uint8_t amount[32];
    uint8_t ticker_1[MAX_TICKER_LEN];
    uint8_t ticker_2[MAX_TICKER_LEN];
    uint8_t decimals;
    uint8_t target;
} erc20_parameters_t;

typedef struct ticker_binding_t {
    char ticker1[MAX_TICKER_LEN];
    char ticker2[MAX_TICKER_LEN];
} ticker_binding_t;

#define NUM_COMPOUND_BINDINGS 9
const ticker_binding_t const COMPOUND_BINDINGS[NUM_COMPOUND_BINDINGS] = {
    {"DAI", "CDAI"},
    {"WETH", "CETH"},
    {"USDC", "CUSDC"},
    {"ZRX", "CZRX"},
    {"USDT", "CUSDT"},
    {"WBTC", "CBTC"},
    {"BAT", "CBAT"},
    {"REPv2", "CREP"},
    {"SAI", "CSAI"},
};

bool check_token_binding(char *ticker1,
                         char *ticker2,
                         const ticker_binding_t *bindings,
                         size_t num_bindings) {
    for (size_t i = 0; i < num_bindings; i++) {
        ticker_binding_t *binding = (ticker_binding_t *) PIC(&bindings[i]);
        if (strncmp(binding->ticker1, ticker1, strnlen(binding->ticker1, MAX_TICKER_LEN)) == 0 &&
            strncmp(binding->ticker2, ticker2, strnlen(binding->ticker2, MAX_TICKER_LEN)) == 0) {
            return true;
        }
    }
    return false;
}

bool erc20_plugin_available_check() {
#ifdef HAVE_STARKWARE
    if (quantumSet) {
        switch (dataContext.tokenContext.quantumType) {
            case STARK_QUANTUM_LEGACY:
            case STARK_QUANTUM_ETH:
            case STARK_QUANTUM_ERC20:
            case STARK_QUANTUM_MINTABLE_ERC20:
                return true;
            default:
                return false;
        }
    }
#endif
    return true;
}

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
                msg->tokenLookup2 = context->destinationAddress;
                msg->numScreens = 2;
                msg->uiType = ETH_UI_TYPE_GENERIC;
                msg->result = ETH_PLUGIN_RESULT_OK;
            }
        } break;

        case ETH_PLUGIN_PROVIDE_TOKEN: {
            ethPluginProvideToken_t *msg = (ethPluginProvideToken_t *) parameters;
            erc20_parameters_t *context = (erc20_parameters_t *) msg->pluginContext;
            PRINTF("erc20 plugin provide token 1: %d - 2: %d\n",
                   (msg->token1 != NULL),
                   (msg->token2 != NULL));
            if (msg->token1 != NULL) {
                context->target = TARGET_ADDRESS;
                strcpy((char *) context->ticker_1, (char *) msg->token1->ticker);
                context->decimals = msg->token1->decimals;
                if (context->selectorIndex == ERC20_APPROVE) {
                    if (msg->token2 != NULL) {
                        context->target = TARGET_CONTRACT;
                        strcpy((char *) context->ticker_2, (char *) msg->token2->ticker);
                        // test if we're doing a Compound allowance
                        if (check_token_binding((char *) msg->token1->ticker,
                                                (char *) msg->token2->ticker,
                                                COMPOUND_BINDINGS,
                                                NUM_COMPOUND_BINDINGS)) {
                            context->target = TARGET_COMPOUND;
                        }
                    }
                }
                msg->result = ETH_PLUGIN_RESULT_OK;
            } else {
                msg->result = ETH_PLUGIN_RESULT_FALLBACK;
            }
        } break;

        case ETH_PLUGIN_QUERY_CONTRACT_ID: {
            ethQueryContractID_t *msg = (ethQueryContractID_t *) parameters;
            strcpy(msg->name, "Type");
            strcpy(msg->version, "Approve");
            msg->result = ETH_PLUGIN_RESULT_OK;
        } break;

        case ETH_PLUGIN_QUERY_CONTRACT_UI: {
            ethQueryContractUI_t *msg = (ethQueryContractUI_t *) parameters;
            erc20_parameters_t *context = (erc20_parameters_t *) msg->pluginContext;
            switch (msg->screenIndex) {
                case 0:
                    strcpy(msg->title, "Amount");
                    if (ismaxint(context->amount, sizeof(context->amount))) {
                        strcpy(msg->msg, "Unlimited ");
                        strcat(msg->msg, (char *) context->ticker_1);
                    } else {
                        amountToString(context->amount,
                                       sizeof(context->amount),
                                       context->decimals,
                                       (char *) context->ticker_1,
                                       msg->msg,
                                       100);
                    }
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;
                case 1:
                    if (context->target >= TARGET_CONTRACT) {
                        strcpy(msg->title, "Contract");
                        if (context->target == TARGET_COMPOUND) {
                            strcpy(msg->msg, "Compound ");
                            strcat(msg->msg,
                                   (char *) context->ticker_2 +
                                       1);  // remove the 'c' char at beginning of compound ticker
                        } else {
                            strcpy(msg->msg, (char *) context->ticker_2);
                        }
                    } else {
                        strcpy(msg->title, "Address");
                        msg->msg[0] = '0';
                        msg->msg[1] = 'x';
                        getEthAddressStringFromBinary(context->destinationAddress,
                                                      (uint8_t *) msg->msg + 2,
                                                      msg->pluginSharedRW->sha3,
                                                      chainConfig);
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
