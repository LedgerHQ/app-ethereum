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

static void handle_num_paths(ethPluginProvideParameter_t *msg, paraswap_parameters_t *context) {
    context->num_paths = msg->parameter[PARAMETER_LENGTH - 1];
    PRINTF("NUM PATHS: %d\n", context->num_paths);
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
    PRINTF("eth2 plugin provide parameter %d %.*H\n",
           msg->parameterOffset,
           PARAMETER_LENGTH,
           msg->parameter);

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
                switch (context->current_param) {
                    case AMOUNT_IN:
                        handle_amount_sent(msg, context);
                        context->current_param = AMOUNT_OUT;
                        break;
                    case AMOUNT_OUT:
                        handle_amount_received(msg, context);
                        context->current_param = NUM_PATHS;
                        context->skip = 2;  // Need to skip two fields before getting to num_paths
                        break;
                    case NUM_PATHS:
                        handle_num_paths(msg, context);
                        context->current_param = TOKEN_SENT;
                        break;
                    case TOKEN_SENT:
                        handle_token_sent(msg, context);
                        context->skip = context->num_paths - 2; // -2 because we won't be skipping the first one and the last one.
                        context->current_param = TOKEN_RECEIVED;
                        break;
                    case TOKEN_RECEIVED:
                        handle_token_received(msg, context);
                        context->num_paths = 0;
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
                switch (context->current_param) {
                    case AMOUNT_IN:
                        handle_amount_sent(msg, context);
                        context->current_param = AMOUNT_OUT;
                        break;
                    case AMOUNT_OUT:
                        handle_amount_received(msg, context);
                        context->current_param = NUM_PATHS;
                        context->skip = 2;  // Need to skip two fields
                        break;
                    case NUM_PATHS:
                        handle_num_paths(msg, context);
                        context->current_param = TOKEN_SENT;
                        break;
                    case TOKEN_SENT:
                        handle_token_sent(msg, context);
                        context->current_param = TOKEN_RECEIVED;
                        context->skip = context->num_paths - 2; // -2 because we won't be skipping the first one and the last one.
                        break;
                    case TOKEN_RECEIVED:
                        handle_token_received(msg, context);
                        context->num_paths = 0;
                        break;
                    default:
                        PRINTF("Unsupported param\n");
                        msg->result = ETH_PLUGIN_RESULT_ERROR;
                        break;
                }
                break;
            }
        }
    }
}