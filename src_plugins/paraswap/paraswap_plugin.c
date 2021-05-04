#include "paraswap_plugin.h"

// Paraswap uses `0xeeeee` as a dummy address to represent ETH.
const uint8_t PARASWAP_ETH_ADDRESS[] = {0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
                                        0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee};

// Prepend `dest` with `ticker`.
// Dest must be big enough to hold `ticker` + `dest` + `\0`.
static void prepend_ticker(char *dest, uint8_t destsize, char *ticker) {
    if (dest == NULL || ticker == NULL) {
        THROW(0x6503);
    }
    uint8_t ticker_len = strlen(ticker);
    uint8_t dest_len = strlen(dest);

    if (dest_len + ticker_len >= destsize) {
        THROW(0x6504);
    }

    // Right shift the string by `ticker_len` bytes.
    while (dest_len != 0) {
        dest[dest_len + ticker_len] = dest[dest_len];  // First iteration will copy the \0
        dest_len--;
    }
    // Don't forget to null terminate the string.
    dest[ticker_len] = dest[0];

    // Copy the ticker to the beginning of the string.
    memcpy(dest, ticker, ticker_len);
}

// Called once to init.
static void handle_init_contract(void *parameters) {
    ethPluginInitContract_t *msg = (ethPluginInitContract_t *) parameters;
    paraswap_parameters_t *context = (paraswap_parameters_t *) msg->pluginContext;
    memset(context, 0, sizeof(*context));
    context->valid = 1;

    size_t i;
    for (i = 0; i < NUM_PARASWAP_SELECTORS; i++) {
        if (memcmp((uint8_t *) PIC(PARASWAP_SELECTORS[i]), msg->selector, SELECTOR_SIZE) == 0) {
            context->selectorIndex = i;
            break;
        }
    }

    PRINTF("\n\nindex: %d\n\n", context->selectorIndex);
    // Set `next_param` to be the first field we expect to parse.
    switch (context->selectorIndex) {
        case BUY_ON_UNI:
        case SWAP_ON_UNI:
            context->next_param = AMOUNT_SENT;
            break;
        case BUY_ON_UNI_FORK:
        case SWAP_ON_UNI_FORK:
            context->skip = 2;  // Skip the first two parameters (factory and initCode).
            context->next_param = AMOUNT_SENT;
            break;
        case SIMPLE_BUY:
        case SIMPLE_SWAP:
        case MULTI_SWAP:
        case BUY:
        case MEGA_SWAP:
            context->next_param = TOKEN_SENT;
            PRINTF("INIT: TOKEN_SENT\n");
            break;
        default:
            PRINTF("Missing selectorIndex\n");
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            return;
    }

    msg->result = ETH_PLUGIN_RESULT_OK;
}

static void handle_finalize(void *parameters) {
    ethPluginFinalize_t *msg = (ethPluginFinalize_t *) parameters;
    paraswap_parameters_t *context = (paraswap_parameters_t *) msg->pluginContext;
    PRINTF("eth2 plugin finalize\n");
    if (context->valid) {
        msg->numScreens = 2;
        if (context->selectorIndex == SIMPLE_SWAP || context->selectorIndex == SIMPLE_BUY)
            // An addiitonal screen is required to display the `beneficiary` field.
            msg->numScreens += 1;
        if (ADDRESS_IS_ETH(context->contract_address_sent) == 0) {
            // Address is not ETH so we will need to look up the token in the CAL.
            msg->tokenLookup1 = context->contract_address_sent;
        }
        if (ADDRESS_IS_ETH(context->contract_address_received) == 0) {
            // Address is not ETH so we will need to look up the token in the CAL.
            msg->tokenLookup2 = context->contract_address_received;
        }

        msg->uiType = ETH_UI_TYPE_GENERIC;
        msg->result = ETH_PLUGIN_RESULT_OK;
    } else {
        msg->result = ETH_PLUGIN_RESULT_FALLBACK;
    }
}

static void handle_provide_token(void *parameters) {
    ethPluginProvideToken_t *msg = (ethPluginProvideToken_t *) parameters;
    paraswap_parameters_t *context = (paraswap_parameters_t *) msg->pluginContext;
    PRINTF("PARASWAP plugin provide token: 0x%p, 0x%p\n", msg->token1, msg->token2);
    switch (context->selectorIndex) {
        case SWAP_ON_UNI_FORK:
        case SWAP_ON_UNI: {
            if (msg->token1 != NULL) {
                context->decimals_sent = msg->token1->decimals;
                strncpy(context->ticker_sent,
                        (char *) msg->token1->ticker,
                        sizeof(context->ticker_sent));
            } else {
                context->decimals_sent = WEI_TO_ETHER;
                uint8_t *ticker = (uint8_t *) PIC(chainConfig->coinName);
                strncpy(context->ticker_sent, (char *) ticker, sizeof(context->ticker_sent));
            }
            if (msg->token2 != NULL) {
                context->decimals_received = msg->token2->decimals;
                strncpy(context->ticker_received,
                        (char *) msg->token2->ticker,
                        sizeof(context->ticker_received));
            } else {
                context->decimals_received = WEI_TO_ETHER;
                uint8_t *ticker = (uint8_t *) PIC(chainConfig->coinName);
                strncpy(context->ticker_received,
                        (char *) ticker,
                        sizeof(context->ticker_received));
            }
            msg->result = ETH_PLUGIN_RESULT_OK;
            break;
        }
        default:
            PRINTF("NOT PROVIDING TOKEN FOR SWAPONUNI\n");
            msg->result = ETH_PLUGIN_RESULT_FALLBACK;
            break;
    }
}

static void handle_query_contract_id(void *parameters) {
    ethQueryContractID_t *msg = (ethQueryContractID_t *) parameters;
    paraswap_parameters_t *context = (paraswap_parameters_t *) msg->pluginContext;

    strcpy(msg->name, "Paraswap");
    msg->nameLength = sizeof("Paraswap");  // scott
    switch (context->selectorIndex) {
        case MULTI_SWAP:
        case SIMPLE_SWAP:
        case SWAP_ON_UNI_FORK:
        case SWAP_ON_UNI:
            strcpy(msg->version, "Swap");
            break;
        case SIMPLE_BUY:
        case BUY_ON_UNI_FORK:
        case BUY_ON_UNI:
            strcpy(msg->version, "Buy");
            break;
    }

    msg->versionLength = strlen(msg->version) + 1;
    msg->result = ETH_PLUGIN_RESULT_OK;
}

static void handle_query_contract_ui(void *parameters) {
    ethQueryContractUI_t *msg = (ethQueryContractUI_t *) parameters;
    paraswap_parameters_t *context = (paraswap_parameters_t *) msg->pluginContext;
    memset(msg->title, 0, 100);  // 100 ? degueu
    memset(msg->msg, 0, 40);     // 40 ? deugue
    msg->result = ETH_PLUGIN_RESULT_OK;
    switch (msg->screenIndex) {
        case 0: {
            strcpy(msg->title, "Send");
            adjustDecimals((char *) context->amount_sent,
                           strlen((char *) context->amount_sent),
                           msg->msg,
                           40,  // Len scott ?
                           context->decimals_sent);
            prepend_ticker(msg->msg, 40, context->ticker_sent);
        } break;
        case 1: {
            strcpy(msg->title, "Receive");
            adjustDecimals((char *) context->amount_received,
                           strlen((char *) context->amount_received),
                           msg->msg,
                           40,  // len scott ?
                           context->decimals_received);
            prepend_ticker(msg->msg, 40, context->ticker_received);
            break;
        }
        case 2: {
            strcpy(msg->title, "Beneficiary");
            msg->msg[0] = '0';
            msg->msg[1] = 'x';
            getEthAddressStringFromBinary((uint8_t *) context->beneficiary,
                                          (uint8_t *) msg->msg + 2,
                                          &global_sha3,
                                          chainConfig);
            break;
        }
        default:
            PRINTF("GET REKT\n");  // scott
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

void paraswap_plugin_call(int message, void *parameters) {
    switch (message) {
        case ETH_PLUGIN_INIT_CONTRACT:
            handle_init_contract(parameters);
            break;
        case ETH_PLUGIN_PROVIDE_PARAMETER:
            handle_provide_parameter(parameters);
            break;
        case ETH_PLUGIN_FINALIZE:
            handle_finalize(parameters);
            break;
        case ETH_PLUGIN_PROVIDE_TOKEN:
            handle_provide_token(parameters);
            break;
        case ETH_PLUGIN_QUERY_CONTRACT_ID:
            handle_query_contract_id(parameters);
            break;
        case ETH_PLUGIN_QUERY_CONTRACT_UI:
            handle_query_contract_ui(parameters);
            break;
        default:
            PRINTF("Unhandled message %d\n", message);
    }
}
