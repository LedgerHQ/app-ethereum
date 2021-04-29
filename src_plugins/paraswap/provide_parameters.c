#include "paraswap_plugin.h"

// Store the amount sent in the form of a string, without any ticker or decimals. These will be added when displaying.
static void handle_amount_sent(ethPluginProvideParameter_t *msg, paraswap_parameters_t *context) {
    memset(context->amount_sent, 0, sizeof(context->amount_sent));

    uint8_t i = 0;
    // Skip the leading zeros.
    while (msg->parameter[i] == 0) {
        i++;
        if (i >= PARAMETER_LENGTH) {
            PRINTF("SCOTT GET REKT\n"); // THROW
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

// Store the amount received in the form of a string, without any ticker or decimals. These will be added when displaying.
static void handle_amount_received(ethPluginProvideParameter_t *msg,
                                   paraswap_parameters_t *context) {
    memset(context->amount_received, 0, sizeof(context->amount_received));

    // Skip the leading zeros.
    uint8_t i = 0;
    while (msg->parameter[i] == 0) {
        i++;
        if (i >= PARAMETER_LENGTH) {
            PRINTF("SCOTT GET REKT\n"); // throw
        }
    }
    // Convert to string.
    amountToString(&msg->parameter[i],
                   PARAMETER_LENGTH - i,
                   0, // No decimals
                   "", // No ticker
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
    PRINTF("eth2 plugin provide parameter %d %.*H\n", msg->parameterOffset, PARAMETER_LENGTH, msg->parameter);
    msg->result = ETH_PLUGIN_RESULT_OK;
    switch (context->selectorIndex) {
        case BUY_ON_UNI:
        case SWAP_ON_UNI: {
            switch (msg->parameterOffset) {
                case 4 + (PARAMETER_LENGTH * 0):  // amountIn
                    handle_amount_sent(msg, context);
                    break;
                case 4 + (PARAMETER_LENGTH * 1):  // amountOutMin
                    handle_amount_received(msg, context);
                    break;
                case 4 + (PARAMETER_LENGTH * 2):  // 0x00..080 unknown
                case 4 + (PARAMETER_LENGTH * 3):  // 0x00..001 unknown
                    break;
                case 4 + (PARAMETER_LENGTH * 4):  // number of elements in the path
                    handle_num_paths(msg, context);
                    break;
                case 4 + (PARAMETER_LENGTH * 5):  // From token
                    handle_token_sent(msg, context);
                    break;
                default:
                {
                    if (msg->parameterOffset ==
                        4 + (PARAMETER_LENGTH * (5 + context->num_paths - 1)))  // To token
                    {
                        handle_token_received(msg, context);
                    } else {
                        PRINTF("Unsupported offset\n");
                    }
                    break;
                }
            }
            break;
        }
        case BUY_ON_UNI_FORK:
        case SWAP_ON_UNI_FORK: {
            switch (msg->parameterOffset) {
                case 4 + (PARAMETER_LENGTH * 0):  // factory
                case 4 + (PARAMETER_LENGTH * 1):  // initCode
                    break;
                case 4 + (PARAMETER_LENGTH * 2):  // amountIn
                    handle_amount_sent(msg, context);
                    break;
                case 4 + (PARAMETER_LENGTH * 3):  // amountOutMin
                    handle_amount_received(msg, context);
                    break;
                case 4 + (PARAMETER_LENGTH * 4): // ?
                case 4 + (PARAMETER_LENGTH * 5): // ?
                    break;
                case 4 + (PARAMETER_LENGTH * 6):  // num paths
                    handle_num_paths(msg, context);
                    break;
                case 4 + (PARAMETER_LENGTH * 7):  // first path
                    handle_token_sent(msg, context);
                    break;
                default:
                {
                    uint32_t last_path_offset = 4 + (PARAMETER_LENGTH * (7 + context->num_paths - 1));
                    if (msg->parameterOffset < last_path_offset) {
                        PRINTF("Parsing paths number %d", (msg->parameterOffset - 4) / PARAMETER_LENGTH - 7);
                    }
                    else if (msg->parameterOffset == last_path_offset) {
                        handle_token_received(msg, context);
                    } else {
                        PRINTF("Unsupported offset\n");
                    }
                    break;
                }
            }
            break;
        }
    }
}