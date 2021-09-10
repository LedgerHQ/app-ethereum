#include "erc721_plugin.h"

static void copy_address(uint8_t *dst, uint8_t *parameter) {
    memmove(dst, parameter + PARAMETER_LENGTH - ADDRESS_LENGTH, ADDRESS_LENGTH);
}

static void copy_parameter(uint8_t *dst, uint8_t *parameter) {
    memmove(dst, parameter, PARAMETER_LENGTH);
}

void handle_provide_parameter(void *parameters) {
    ethPluginProvideParameter_t *msg = (ethPluginProvideParameter_t *) parameters;
    erc721_parameters_t *context = (erc721_parameters_t *) msg->pluginContext;

    PRINTF("erc721 plugin provide parameter %d %.*H\n",
           msg->parameterOffset,
           PARAMETER_LENGTH,
           msg->parameter);

    msg->result = ETH_PLUGIN_RESULT_SUCCESSFUL;
    switch (msg->parameterOffset) {
        case OPERATOR:
            copy_address(context->address, msg->parameter);
            context->next_param = TOKEN_ID;
            break;
        case TOKEN_ID:
            copy_parameter(context->tokenId, msg->parameter);
            context->next_param = NONE;
            break;
        default:
            PRINTF("Unhandled parameter offset\n");
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}