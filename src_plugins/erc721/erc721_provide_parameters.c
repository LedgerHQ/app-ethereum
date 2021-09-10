#include "erc721_plugin.h"

void handle_provide_parameter(void *parameters) {
    ethPluginProvideParameter_t *msg = (ethPluginProvideParameter_t *) parameters;
    erc721_parameters_t *context = (erc721_parameters_t *) msg->pluginContext;

    PRINTF("erc721 plugin provide parameter %d %.*H\n",
           msg->parameterOffset,
           PARAMETER_LENGTH,
           msg->parameter);

    msg->result = ETH_PLUGIN_RESULT_SUCCESSFUL;
    switch (msg->parameterOffset) {
        case SELECTOR_SIZE:
            memmove(context->address,
                    msg->parameter + PARAMETER_LENGTH - ADDRESS_LENGTH,
                    ADDRESS_LENGTH);
            break;
        case SELECTOR_SIZE + PARAMETER_LENGTH:
            memmove(context->tokenId, msg->parameter, PARAMETER_LENGTH);
            break;
        default:
            PRINTF("Unhandled parameter offset\n");
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}