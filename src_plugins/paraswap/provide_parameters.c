#include "paraswap_plugin.h"

// Store the amount sent in the form of a string, without any ticker or decimals. These will be
// added when displaying.
static void handle_amount_sent(ethPluginProvideParameter_t *msg, paraswap_parameters_t *context) {
    memset(context->amount_sent, 0, sizeof(context->amount_sent));

    uint8_t i = 0;
    // Skip the leading zeros.
    while (msg->parameter[i] == 0) {
        i++;
        if (i >= PARAMETER_LENGTH) {
            PRINTF("SCOTT GET REKT\n");  // THROW
        }
    }

    // Convert to string.
    amountToString(&msg->parameter[i],
                   PARAMETER_LENGTH - i,
                   0,
                   "",
                   (char *) context->amount_sent,
                   sizeof(context->amount_sent));
}

// Store the amount received in the form of a string, without any ticker or decimals. These will be
// added when displaying.
static void handle_amount_received(ethPluginProvideParameter_t *msg,
                                   paraswap_parameters_t *context) {
    memset(context->amount_received, 0, sizeof(context->amount_received));

    // Skip the leading zeros.
    uint8_t i = 0;
    while (msg->parameter[i] == 0) {
        i++;
        if (i >= PARAMETER_LENGTH) {
            PRINTF("SCOTT GET REKT\n");  // throw
        }
    }
    // Convert to string.
    amountToString(&msg->parameter[i],
                   PARAMETER_LENGTH - i,
                   0,   // No decimals
                   "",  // No ticker
                   (char *) context->amount_received,
                   sizeof(context->amount_received));
}

static void handle_beneficiary(ethPluginProvideParameter_t *msg, paraswap_parameters_t *context) {
    memset(context->contract_address_sent, 0, sizeof(context->contract_address_sent));
    memcpy(context->contract_address_sent,
           &msg->parameter[PARAMETER_LENGTH - ADDRESS_LENGTH],
           sizeof(context->contract_address_sent));
    PRINTF("BENEFICIARY: %.*H\n", ADDRESS_LENGTH, context->beneficiary);
}

static void handle_list_len(ethPluginProvideParameter_t *msg, paraswap_parameters_t *context) {
    context->list_len = msg->parameter[PARAMETER_LENGTH - 1];
    PRINTF("LIST LEN: %d\n", context->list_len);
}

static void handle_token_sent(ethPluginProvideParameter_t *msg, paraswap_parameters_t *context) {
    memset(context->contract_address_sent, 0, sizeof(context->contract_address_sent));
    memcpy(context->contract_address_sent,
           &msg->parameter[PARAMETER_LENGTH - ADDRESS_LENGTH],
           sizeof(context->contract_address_sent));
    PRINTF("TOKEN SENT: %.*H\n", ADDRESS_LENGTH, context->contract_address_sent);
}

static void handle_token_received(ethPluginProvideParameter_t *msg,
                                  paraswap_parameters_t *context) {
    memset(context->contract_address_received, 0, sizeof(context->contract_address_received));
    memcpy(context->contract_address_received,
           &msg->parameter[PARAMETER_LENGTH - ADDRESS_LENGTH],
           sizeof(context->contract_address_received));
    PRINTF("TOKEN RECIEVED: %.*H\n", ADDRESS_LENGTH, context->contract_address_received);
}

void handle_provide_parameter(void *parameters) {
    ethPluginProvideParameter_t *msg = (ethPluginProvideParameter_t *) parameters;
    paraswap_parameters_t *context = (paraswap_parameters_t *) msg->pluginContext;
    // PRINTF("eth2 plugin provide parameter %d %.*H\n",
        //    msg->parameterOffset,
        //    PARAMETER_LENGTH,
        //    msg->parameter);

    msg->result = ETH_PLUGIN_RESULT_OK;

    if (context->skip) {
        // Skip this step, and don't forget ton decrease skipping counter.
        PRINTF("SKIPPING: %d\n", context->skip);
        context->skip--;
    } else {
        PRINTF("NOT SKIPPING: INDEX: %d\n", context->selectorIndex);
        switch (context->selectorIndex) {
            case BUY_ON_UNI:
            case SWAP_ON_UNI: {
                switch (context->next_param) {
                    case AMOUNT_SENT:  // amountIn
                        handle_amount_sent(msg, context);
                        context->next_param = AMOUNT_RECEIVED;
                        break;
                    case AMOUNT_RECEIVED:  // amountOut
                        handle_amount_received(msg, context);
                        context->next_param = PATH;
                        context->skip = 2;  // Need to skip two fields before getting to list_len
                        break;
                    case PATH:  // len(path)
                        handle_list_len(msg, context);
                        context->next_param = TOKEN_SENT;
                        break;
                    case TOKEN_SENT:  // path[0]
                        handle_token_sent(msg, context);
                        context->skip =
                            context->list_len -
                            2;  // -2 because we won't be skipping the first one and the last one.
                        context->next_param = TOKEN_RECEIVED;
                        break;
                    case TOKEN_RECEIVED:  // path[len(path) - 1]
                        handle_token_received(msg, context);
                        context->list_len = 0;
                        break;
                    default:
                        PRINTF("Unsupported param\n");
                        msg->result = ETH_PLUGIN_RESULT_ERROR;
                        break;
                }
                break;
            }
            case BUY_ON_UNI_FORK:
            case SWAP_ON_UNI_FORK: {
                switch (context->next_param) {
                    case AMOUNT_SENT:  // amountInMax
                        handle_amount_sent(msg, context);
                        context->next_param = AMOUNT_RECEIVED;
                        break;
                    case AMOUNT_RECEIVED:  // amountOut
                        handle_amount_received(msg, context);
                        context->next_param = PATH;
                        context->skip = 2;  // Need to skip two fields
                        break;
                    case PATH:  // len(path)
                        handle_list_len(msg, context);
                        context->next_param = TOKEN_SENT;
                        break;
                    case TOKEN_SENT:  // path[0]
                        handle_token_sent(msg, context);
                        context->next_param = TOKEN_RECEIVED;
                        context->skip =
                            context->list_len -
                            2;  // -2 because we won't be skipping the first one and the last one.
                        break;
                    case TOKEN_RECEIVED:  // path[len(path) - 1]
                        handle_token_received(msg, context);
                        context->list_len = 0;
                        break;
                    default:
                        PRINTF("Unsupported param\n");
                        msg->result = ETH_PLUGIN_RESULT_ERROR;
                        break;
                }
                break;
            }

            case SIMPLE_BUY:
            case SIMPLE_SWAP: {
                switch (context->next_param) {
                    case TOKEN_SENT:  // fromToken
                        handle_token_sent(msg, context);
                        context->next_param = TOKEN_RECEIVED;
                        break;
                    case TOKEN_RECEIVED:  // toToken
                        handle_token_received(msg, context);
                        context->next_param = AMOUNT_RECEIVED;
                        break;
                    case AMOUNT_SENT:  // fromAmount
                        handle_amount_sent(msg, context);
                        context->next_param = AMOUNT_RECEIVED;
                        break;
                    case AMOUNT_RECEIVED:  // toAmount
                        handle_amount_received(msg, context);
                        context->next_param = EXPECTED_AMOUNT;
                        break;
                    case EXPECTED_AMOUNT:
                        context->skip = 2;
                        context->next_param = CALLEES;
                        break;
                    case CALLEES:
                        handle_list_len(msg, context);
                        context->next_param = EXCHANGE_DATA;
                        context->skip = context->list_len;
                        break;
                    case EXCHANGE_DATA:  // verify because bytes?
                        context->next_param = START_INDEXES;
                        context->skip = 2;
                        break;
                    case START_INDEXES:
                        handle_list_len(msg, context);
                        context->next_param = VALUES;
                        context->skip =
                            context->list_len + 2;  // + 2 because next field is a list too
                        break;
                    case VALUES:
                        handle_list_len(msg, context);
                        context->skip = context->list_len;
                        context->next_param = BENEFICIARY;
                        break;
                    case BENEFICIARY:
                        handle_beneficiary(msg, context);
                        context->skip = 0;
                        break;
                    default:
                        PRINTF("Param not supported\n");
                        msg->result = ETH_PLUGIN_RESULT_ERROR;
                        break;
                }
                break;
            }

            case MULTI_SWAP: {
                PRINTF("MULTI_SWAP: %d\n", context->next_param);
                switch (context->next_param) {
                    case TOKEN_SENT:  // fromToken
                        handle_token_sent(msg, context);
                        context->next_param = AMOUNT_SENT;
                        break;
                    case AMOUNT_SENT:  // fromAmount
                        handle_amount_sent(msg, context);
                        context->next_param = AMOUNT_RECEIVED;
                        break;
                    case AMOUNT_RECEIVED:  // toAmount
                        handle_amount_received(msg, context);
                        context->next_param = EXPECTED_AMOUNT;
                        break;
                    case EXPECTED_AMOUNT:  // expectedAmount
                        context->next_param = BENEFICIARY;
                        break;
                    case BENEFICIARY:
                        handle_beneficiary(msg, context);
                        context->next_param = PATH;
                        context->skip =
                            4;  // Referrer, useReduxTokens and two fields because of list.
                        break;
                    case PATH:  // len(path) SCOTT NEED REWORK for Path[]
                        handle_list_len(msg, context);
                        context->skip = context->list_len - 1;
                        context->next_param = TOKEN_RECEIVED;
                        break;
                    case TOKEN_RECEIVED:
                        handle_token_received(msg, context);
                        break;
                    default:
                        PRINTF("Param not supported\n");
                        msg->result = ETH_PLUGIN_RESULT_ERROR;
                        break;
                }
                break;
            }

            case BUY: {
                switch (context->next_param) {
                    case TOKEN_SENT:  // fromToken
                        handle_token_sent(msg, context);
                        context->next_param = TOKEN_RECEIVED;
                        break;
                    case TOKEN_RECEIVED:  // toToken
                        handle_token_received(msg, context);
                        context->next_param = AMOUNT_SENT;
                        break;
                    case AMOUNT_SENT:
                        handle_amount_sent(msg, context);
                        context->next_param = AMOUNT_RECEIVED;
                    case AMOUNT_RECEIVED:  // toAmount
                        handle_amount_received(msg, context);
                        context->next_param = BENEFICIARY;
                        break;
                    case BENEFICIARY:
                        handle_beneficiary(msg, context);
                        break;
                    default:
                        PRINTF("Param not supported\n");
                        msg->result = ETH_PLUGIN_RESULT_ERROR;
                        break;
                }
                break;
            }

                // case MEGA_SWAP: scott
        }
    }
}