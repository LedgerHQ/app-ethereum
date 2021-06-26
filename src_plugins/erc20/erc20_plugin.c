#include <string.h>
#include "eth_plugin_internal.h"
#include "eth_plugin_handler.h"
#include "shared_context.h"
#include "ethUtils.h"
#include "ethUstream.h"
#include "utils.h"

typedef enum { ERC20_TRANSFER = 0, ERC20_APPROVE } erc20Selector_t;

typedef enum { TARGET_ADDRESS = 0, TARGET_CONTRACT } targetType_t;

#define MAX_CONTRACT_NAME_LEN 15

typedef struct erc20_parameters_t {
    uint8_t selectorIndex;
    uint8_t destinationAddress[21];
    uint8_t amount[INT256_LENGTH];
    uint8_t ticker[MAX_TICKER_LEN];
    uint8_t decimals;
    uint8_t target;
    uint8_t contract_name[MAX_CONTRACT_NAME_LEN];
} erc20_parameters_t;

typedef struct contract_t {
    char name[MAX_CONTRACT_NAME_LEN];
    uint8_t address[ADDRESS_LENGTH];
} contract_t;

#define NUM_CONTRACTS 12
const contract_t const CONTRACTS[NUM_CONTRACTS] = {
    // Compound
    {"Compound DAI", {0x5d, 0x3a, 0x53, 0x6e, 0x4d, 0x6d, 0xbd, 0x61, 0x14, 0xcc,
                      0x1e, 0xad, 0x35, 0x77, 0x7b, 0xab, 0x94, 0x8e, 0x36, 0x43}},
    {"Compound ETH", {0x4d, 0xdc, 0x2d, 0x19, 0x39, 0x48, 0x92, 0x6d, 0x02, 0xf9,
                      0xb1, 0xfe, 0x9e, 0x1d, 0xaa, 0x07, 0x18, 0x27, 0x0e, 0xd5}},
    {"Compound USDC", {0x39, 0xaa, 0x39, 0xc0, 0x21, 0xdf, 0xba, 0xe8, 0xfa, 0xc5,
                       0x45, 0x93, 0x66, 0x93, 0xac, 0x91, 0x7d, 0x5e, 0x75, 0x63}},
    {"Compound ZRX", {0xb3, 0x31, 0x9f, 0x5d, 0x18, 0xbc, 0x0d, 0x84, 0xdd, 0x1b,
                      0x48, 0x25, 0xdc, 0xde, 0x5d, 0x5f, 0x72, 0x66, 0xd4, 0x07}},
    {"Compound USDT", {0xf6, 0x50, 0xc3, 0xd8, 0x8d, 0x12, 0xdb, 0x85, 0x5b, 0x8b,
                       0xf7, 0xd1, 0x1b, 0xe6, 0xc5, 0x5a, 0x4e, 0x07, 0xdc, 0xc9}},
    {"Compound WBTC", {0xc1, 0x1b, 0x12, 0x68, 0xc1, 0xa3, 0x84, 0xe5, 0x5c, 0x48,
                       0xc2, 0x39, 0x1d, 0x8d, 0x48, 0x02, 0x64, 0xa3, 0xa7, 0xf4}},
    {"Compound BAT", {0x6c, 0x8c, 0x6b, 0x02, 0xe7, 0xb2, 0xbe, 0x14, 0xd4, 0xfa,
                      0x60, 0x22, 0xdf, 0xd6, 0xd7, 0x59, 0x21, 0xd9, 0x0e, 0x4e}},
    {"Compound REP", {0x15, 0x80, 0x79, 0xee, 0x67, 0xfc, 0xe2, 0xf5, 0x84, 0x72,
                      0xa9, 0x65, 0x84, 0xa7, 0x3c, 0x7a, 0xb9, 0xac, 0x95, 0xc1}},
    {"Compound SAI", {0xf5, 0xdc, 0xe5, 0x72, 0x82, 0xa5, 0x84, 0xd2, 0x74, 0x6f,
                      0xaf, 0x15, 0x93, 0xd3, 0x12, 0x1f, 0xca, 0xc4, 0x44, 0xdc}},
    {"Compound UNI", {0x35, 0xa1, 0x80, 0x00, 0x23, 0x0d, 0xa7, 0x75, 0xca, 0xc2,
                      0x48, 0x73, 0xd0, 0x0f, 0xf8, 0x5b, 0xcc, 0xde, 0xd5, 0x50}},

    // Paraswap https://etherscan.io/address/1bd435f3c054b6e901b7b108a0ab7617c808677b
    {"Paraswap", {0x1b, 0xd4, 0x35, 0xf3, 0xc0, 0x54, 0xb6, 0xe9, 0x01, 0xb7,
                  0xb1, 0x08, 0xa0, 0xab, 0x76, 0x17, 0xc8, 0x08, 0x67, 0x7b}},

    // 1inch - https://etherscan.io/address/0x11111112542d85b3ef69ae05771c2dccff4faa26
    {"1inch", {0x11, 0x11, 0x11, 0x12, 0x54, 0x2d, 0x85, 0xb3, 0xef, 0x69,
               0xae, 0x05, 0x77, 0x1c, 0x2d, 0xcc, 0xff, 0x4f, 0xaa, 0x26}}};

bool check_contract(erc20_parameters_t *context) {
    for (size_t i = 0; i < NUM_CONTRACTS; i++) {
        contract_t *contract = (contract_t *) PIC(&CONTRACTS[i]);
        if (memcmp(contract->address, context->destinationAddress, ADDRESS_LENGTH) == 0) {
            strncpy((char *) context->contract_name,
                    contract->name,
                    sizeof(context->contract_name));
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
                strcpy((char *) context->ticker, (char *) msg->token1->ticker);
                context->decimals = msg->token1->decimals;
                if (context->selectorIndex == ERC20_APPROVE) {
                    if (check_contract(context)) {
                        context->target = TARGET_CONTRACT;
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
                        strcat(msg->msg, (char *) context->ticker);
                    } else {
                        amountToString(context->amount,
                                       sizeof(context->amount),
                                       context->decimals,
                                       (char *) context->ticker,
                                       msg->msg,
                                       100);
                    }
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;
                case 1:
                    if (context->target >= TARGET_CONTRACT) {
                        strcpy(msg->title, "Contract");
                        strcpy(msg->msg, (char *) context->contract_name);
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
