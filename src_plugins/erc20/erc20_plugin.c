#include <string.h>
#include "eth_plugin_interface.h"

typedef struct erc20_parameters_t {

	uint8_t destinationAddress[20];
	uint8_t amount[32];

} erc20_parameters_t;

void erc20_plugin_call(int message, void *parameters) {

	switch(message) {
		case ETH_PLUGIN_INIT_CONTRACT: {
			ethPluginInitContract_t *msg = (ethPluginInitContract_t*)parameters;
			PRINTF("erc20 plugin init\n");
			msg->result = ETH_PLUGIN_RESULT_OK;
		}
		break;

		case ETH_PLUGIN_PROVIDE_PARAMETER : {
			ethPluginProvideParameter_t *msg = (ethPluginProvideParameter_t*)parameters;
			erc20_parameters_t *context = (erc20_parameters_t*)msg->pluginContext;
			PRINTF("erc20 plugin provide parameter %d %.*H\n", msg->parameterOffset, 32, msg->parameter);
			switch(msg->parameterOffset) {
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
					break;
			}
		}
		break;

		case ETH_PLUGIN_FINALIZE: {
			ethPluginFinalize_t *msg = (ethPluginFinalize_t*)parameters;
			erc20_parameters_t *context = (erc20_parameters_t*)msg->pluginContext;
			PRINTF("erc20 plugin finalize\n");
			msg->tokenLookup1 = msg->pluginSharedRO->txContent->destination;
			msg->amount = context->amount;
			msg->address = context->destinationAddress;
			msg->uiType = ETH_UI_TYPE_AMOUNT_ADDRESS;
			msg->result = ETH_PLUGIN_RESULT_OK;
		}
		break;

		case ETH_PLUGIN_PROVIDE_TOKEN: {
			ethPluginProvideToken_t *msg = (ethPluginProvideToken_t*)parameters;
			PRINTF("erc20 plugin provide token %d\n", (msg->token1 != NULL));
			msg->result = (msg->token1 != NULL ? ETH_PLUGIN_RESULT_OK : ETH_PLUGIN_RESULT_FALLBACK);
		}
		break;

		default:
			PRINTF("Unhandled message %d\n", message);			
	}
}
