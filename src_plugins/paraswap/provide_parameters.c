#include "paraswap_plugin.h"

static void handle_amount_sent(ethPluginProvideParameter_t *msg, paraswap_parameters_t *context) {
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
}

static void handle_amount_received(ethPluginProvideParameter_t *msg,
                                   paraswap_parameters_t *context) {
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
}

static void handle_num_paths(ethPluginProvideParameter_t *msg, paraswap_parameters_t *context) {
    context->num_paths = msg->parameter[31];  // degueu should be size - 1
    PRINTF("NUM PATHS: %d\n", context->num_paths);
}

static void handle_token_sent(ethPluginProvideParameter_t *msg, paraswap_parameters_t *context) {
    memset(context->contract_address_sent, 0, sizeof(context->contract_address_sent));
    memcpy(context->contract_address_sent,
           &msg->parameter[32 - 20],  // 30 - 20 ?
           sizeof(context->contract_address_sent));
    PRINTF("TOKEN SENT: %.*H\n", 20, context->contract_address_sent);
}

static void handle_token_received(ethPluginProvideParameter_t *msg,
                                  paraswap_parameters_t *context) {
    memset(context->contract_address_received, 0, sizeof(context->contract_address_received));
    memcpy(context->contract_address_received,
           &msg->parameter[32 - 20],  // 32 - 20 ?
           sizeof(context->contract_address_received));
    PRINTF("TOKEN RECIEVED: %.*H\n", 20, context->contract_address_received);
}

void handle_provide_parameter(void *parameters) {
    ethPluginProvideParameter_t *msg = (ethPluginProvideParameter_t *) parameters;
    paraswap_parameters_t *context = (paraswap_parameters_t *) msg->pluginContext;
    PRINTF("eth2 plugin provide parameter %d %.*H\n", msg->parameterOffset, 32, msg->parameter);
    msg->result = ETH_PLUGIN_RESULT_OK;
    switch (context->selectorIndex) {
        case SWAP_ON_UNI: {
            switch (msg->parameterOffset) {
                case 4 + (32 * 0):  // amountIn
                    handle_amount_sent(msg, context);
                    break;
                case 4 + (32 * 1):  // amountOutMin
                    handle_amount_received(msg, context);
                    break;
                case 4 + (32 * 2):  // 0x00..080 unknown
                case 4 + (32 * 3):  // 0x00..001 unknown
                    break;
                case 4 + (32 * 4):  // number of elements in the path
                    handle_num_paths(msg, context);
                    break;
                case 4 + (32 * 5):  // From token
                    handle_token_sent(msg, context);
                    break;
                default:
                {
                    if (msg->parameterOffset ==
                        4 + (32 * (5 + context->num_paths - 1)))  // To token
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
        case SWAP_ON_UNI_FORK: {
            switch (msg->parameterOffset) {
                case 4 + (32 * 0):  // factory
                case 4 + (32 * 1):  // initCode
                    break;
                case 4 + (32 * 2):  // amountIn
                {
                    handle_amount_sent(msg, context);
                    break;
                }
                case 4 + (32 * 3):  // amountOutMin
                {
                    handle_amount_received(msg, context);
                    break;
                }
                case 4 + (32 * 4): // ?
                case 4 + (32 * 5): // ?
                    break;
                case 4 + (32 * 6):  // num paths
                {
                    handle_num_paths(msg, context);
                    break;
                }
                case 4 + (32 * 7):  // first path
                {
                    handle_token_sent(msg, context);
                    break;
                }
                default: {
                    uint32_t last_path_offset = 4 + (32 * (7 + context->num_paths - 1));
                    if (msg->parameterOffset < last_path_offset) {
                        PRINTF("Parsing paths number %d", (msg->parameterOffset - 4) / 32 - 7);
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