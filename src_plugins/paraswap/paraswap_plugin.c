//#ifdef PARSWAP

#include <string.h>
#include "eth_plugin_internal.h"
#include "eth_plugin_handler.h"
#include "shared_context.h"
#include "ethUtils.h"
#include "utils.h"
#include "withdrawal_index.h"

#define ETH2_DEPOSIT_PUBKEY_OFFSET         0x80
#define ETH2_WITHDRAWAL_CREDENTIALS_OFFSET 0xE0
#define ETH2_SIGNATURE_OFFSET              0x120
#define ETH2_DEPOSIT_PUBKEY_LENGTH         0x30
#define ETH2_WITHDRAWAL_CREDENTIALS_LENGTH 0x20
#define ETH2_SIGNATURE_LENGTH              0x60

const uint8_t ETHEREUM_ADDRESS[] = {0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
                                    0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee};

// Returns 1 if corresponding address is the Paraswap address for Ethereum (0xeeeee...).
#define ADDRESS_IS_ETH(_addr) (!memcmp(_addr, ETHEREUM_ADDRESS, 20))

typedef enum {
    PARASWAP_SWAP_ON_UNI = 0,
} paraswapSelector_t;

typedef struct paraswap_parameters_t {
    uint8_t amount_sent[32];            // corresponds to amountIn
    uint8_t contract_address_sent[20];  // use constant
    char ticker_sent[MAX_TICKER_LEN];

    uint8_t amount_received[32];           // corresponds to amountOutMin
    uint8_t contract_address_received[20];  // use constant
    char ticker_received[MAX_TICKER_LEN];

    uint8_t valid;
    uint8_t decimals_sent;      // Not mandatory ?
    uint8_t decimals_received;  // Not mandatory ?
    uint8_t selectorIndex;
} paraswap_parameters_t;

static void prepend_ticker(char *dest, uint8_t destsize, char *ticker) {
    uint8_t ticker_len = strlen(ticker); // check 0

    uint8_t dest_len = strlen(dest); // check 0
    if (dest_len + ticker_len >= destsize) {
        return; // scott throw ?
    }
    // Right shift the string by `ticker_len` bytes.
    while (dest_len != 0) {
        dest[dest_len + ticker_len] = dest[dest_len]; // First iteration will copy the \0
        dest_len--;
    }
    dest[ticker_len] = dest[0];

    // Strcpy the ticker to the beginning of the string.
    uint8_t i = 0;
    while (i < ticker_len) {
        dest[i] = ticker[i];
        i++;
    }
}

// 12345 -> "ETH " E T H   1 2 3 4 5 _
void paraswap_plugin_call(int message, void *parameters) {
    switch (message) {
        case ETH_PLUGIN_INIT_CONTRACT: {
            ethPluginInitContract_t *msg = (ethPluginInitContract_t *) parameters;
            paraswap_parameters_t *context = (paraswap_parameters_t *) msg->pluginContext;
            context->valid = 1;
            size_t i;
            for (i = 0; i < NUM_PARASWAP_SELECTORS; i++) {
                if (memcmp((uint8_t *) PIC(PARASWAP_SELECTORS[i]), msg->selector, SELECTOR_SIZE) ==
                    0) {
                    context->selectorIndex = i;
                    break;
                }
            }
            msg->result = ETH_PLUGIN_RESULT_OK;
            PRINTF("PLUGIN INIT ok\n");
        } break;

        case ETH_PLUGIN_PROVIDE_PARAMETER: {
            ethPluginProvideParameter_t *msg = (ethPluginProvideParameter_t *) parameters;
            paraswap_parameters_t *context = (paraswap_parameters_t *) msg->pluginContext;
            PRINTF("eth2 plugin provide parameter %d %.*H\n",
                   msg->parameterOffset,
                   32,
                   msg->parameter);
            msg->result = ETH_PLUGIN_RESULT_OK;
            switch (msg->parameterOffset) {
                case 4 + (32 * 0):  // amountIn
                {
                    PRINTF("UNKONW3\n\n");
                    ;
                    memset(context->amount_sent, 0, sizeof(context->amount_sent));
                    uint8_t i = 0;
                    while (msg->parameter[i] == 0) {
                        i++;
                        if (i >= 32) {
                            PRINTF("SCOTT GET REKT\n");
                        }
                    }
                    amountToString(&msg->parameter[i],
                                   32 - i,
                                   0,
                                   "",
                                   (char *) context->amount_sent,
                                   sizeof(context->amount_sent));
                    break;
                }
                case 4 + (32 * 1):  // amountOutMin
                {
                    PRINTF("UNKONW2\n\n");
                    memset(context->amount_received, 0, sizeof(context->amount_received));
                    uint8_t i = 0;
                    while (msg->parameter[i] == 0) {
                        i++;
                        if (i >= 32) {
                            PRINTF("SCOTT GET REKT\n");
                        }
                    }
                    amountToString(&msg->parameter[i],
                                   32 - i,
                                   0,
                                   "",
                                   (char *) context->amount_received,
                                   sizeof(context->amount_received));
                    PRINTF("AMOUNT OUT MIN\n\n");
                    ;
                    break;
                }
                case 4 + (32 * 2):  // unknown
                case 4 + (32 * 3):  // unknown
                case 4 + (32 * 4):  // unknown
                    break;
                case 4 + (32 * 5):  // From token
                {
                    memset(context->contract_address_sent,
                           0,
                           sizeof(context->contract_address_sent));
                    memcpy(context->contract_address_sent,
                           &msg->parameter[32 - 20],
                           sizeof(context->contract_address_sent));
                    PRINTF("ADDRESS SENT: %.*H\n", 20, context->contract_address_sent);
                    break;
                }
                case 4 + (32 * 6):  // To token
                {
                    memset(context->contract_address_received,
                           0,
                           sizeof(context->contract_address_received));
                    memcpy(context->contract_address_received,
                           &msg->parameter[32 - 20],
                           sizeof(context->contract_address_received));
                    PRINTF("ADDRESS RECIEVED: %.*H\n", 20, context->contract_address_received);
                    break;
                }
                default:
                    PRINTF("Unhandled parameter offset\n");
                    break;
            }
        } break;

        case ETH_PLUGIN_FINALIZE: {
            ethPluginFinalize_t *msg = (ethPluginFinalize_t *) parameters;
            paraswap_parameters_t *context = (paraswap_parameters_t *) msg->pluginContext;
            PRINTF("eth2 plugin finalize\n");
            if (context->valid) {
                msg->numScreens = 2;
                if (ADDRESS_IS_ETH(context->contract_address_sent) == 0) {
                    PRINTF("Look up sent\n");
                    msg->tokenLookup1 = context->contract_address_sent;
                }
                if (ADDRESS_IS_ETH(context->contract_address_received) == 0) {
                    PRINTF("Look up received\n");
                    msg->tokenLookup2 = context->contract_address_received;
                }
                msg->uiType = ETH_UI_TYPE_GENERIC;
                msg->result = ETH_PLUGIN_RESULT_OK;
            } else {
                msg->result = ETH_PLUGIN_RESULT_FALLBACK;
            }
        } break;

        case ETH_PLUGIN_PROVIDE_TOKEN: {
            ethPluginProvideToken_t *msg = (ethPluginProvideToken_t *) parameters;
            paraswap_parameters_t *context = (paraswap_parameters_t *) msg->pluginContext;
            PRINTF("PARASWAP plugin provide token: %p, %p\n", msg->token1, msg->token2);
            switch (context->selectorIndex) {
                case PARASWAP_SWAP_ON_UNI: {
                    if (msg->token1 != NULL) {
                        context->decimals_sent = msg->token1->decimals;
                        strncpy(context->ticker_sent, (char *)msg->token1->ticker, sizeof(context->ticker_sent));
                    } else {
                        context->decimals_sent = WEI_TO_ETHER;
                        uint8_t *ticker = (uint8_t *) PIC(chainConfig->coinName);
                        strncpy(context->ticker_sent, (char*)ticker, sizeof(context->ticker_sent));
                    }
                    if (msg->token2 != NULL) {
                        context->decimals_received = msg->token2->decimals;
                        strncpy(context->ticker_received, (char *)msg->token2->ticker, sizeof(context->ticker_received));
                    } else {
                        context->decimals_received = WEI_TO_ETHER;
                        uint8_t *ticker = (uint8_t *) PIC(chainConfig->coinName);
                        strncpy(context->ticker_received, (char*)ticker, sizeof(context->ticker_received));
                    }
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;
                }
                default:
                    msg->result = ETH_PLUGIN_RESULT_FALLBACK;
                    break;
            }
        } break;

        case ETH_PLUGIN_QUERY_CONTRACT_ID: {
            ethQueryContractID_t *msg = (ethQueryContractID_t *) parameters;
            strcpy(msg->name, "Paraswap");
            msg->nameLength = sizeof("Paraswap"); // scott
            strcpy(msg->version, "Swap");
            msg->versionLength = strlen(msg->version) + 1;
            msg->result = ETH_PLUGIN_RESULT_OK;
        } break;

        case ETH_PLUGIN_QUERY_CONTRACT_UI: {
            ethQueryContractUI_t *msg = (ethQueryContractUI_t *) parameters;
            paraswap_parameters_t *context = (paraswap_parameters_t*)msg->pluginContext;
            switch (msg->screenIndex) {
                case 0: {
                    strcpy(msg->title, "Send");
                    adjustDecimals((char*)context->amount_sent, strlen(context->amount_sent), msg->msg, 40, context->decimals_sent); // get good size for msg->msg
                    prepend_ticker(msg->msg, 40, context->ticker_sent);
                    msg->result = ETH_PLUGIN_RESULT_OK;
                } break;
                case 1: {
                    strcpy(msg->title, "Receive");
                    adjustDecimals((char*)context->amount_received, strlen(context->amount_received), msg->msg, 40, 6); // get good size for msg->msg
                    prepend_ticker(msg->msg, 40, context->ticker_received);
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;
                }
                default:
                    PRINTF("GET REKT\n"); // scott
                    msg->result = ETH_PLUGIN_RESULT_ERROR;
                    break;
            }
        } break;

        default:
            PRINTF("Unhandled message %d\n", message);
    }
}

//#endif
