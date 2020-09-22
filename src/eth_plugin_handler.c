#include <string.h>
#include "eth_plugin_handler.h"
#include "eth_plugin_internal.h"
#include "shared_context.h"
#include "base64.h"

void eth_plugin_prepare_init(ethPluginInitContract_t *init, uint8_t *selector, uint32_t dataSize) {
	memset((uint8_t*)init, 0, sizeof(ethPluginInitContract_t));
	init->selector = selector;
	init->dataSize = dataSize;
}

void eth_plugin_prepare_provide_parameter(ethPluginProvideParameter_t *provideParameter, uint8_t *parameter, uint32_t parameterOffset) {
	memset((uint8_t*)provideParameter, 0, sizeof(ethPluginProvideParameter_t));
	provideParameter->parameter = parameter;
	provideParameter->parameterOffset = parameterOffset;
}

void eth_plugin_prepare_finalize(ethPluginFinalize_t *finalize) {
	memset((uint8_t*)finalize, 0, sizeof(ethPluginFinalize_t));
}

void eth_plugin_prepare_provide_token(ethPluginProvideToken_t *provideToken, tokenDefinition_t *token1, tokenDefinition_t *token2) {
	memset((uint8_t*)provideToken, 0, sizeof(ethPluginProvideToken_t));
	provideToken->token1 = token1;
	provideToken->token2 = token2;
}

void eth_plugin_prepare_query_contract_ID(ethQueryContractID_t *queryContractID, char *name, uint32_t nameLength, char *version, uint32_t versionLength) {
	memset((uint8_t*)queryContractID, 0, sizeof(ethQueryContractID_t));
	queryContractID->name = name;
	queryContractID->nameLength = nameLength;
	queryContractID->version = version;
	queryContractID->versionLength = versionLength;
}

void eth_plugin_prepare_query_contract_UI(ethQueryContractUI_t *queryContractUI, uint8_t screenIndex, char *title, uint32_t titleLength, char *msg, uint32_t msgLength) {
	memset((uint8_t*)queryContractUI, 0, sizeof(ethQueryContractUI_t));
	queryContractUI->screenIndex = screenIndex;
	queryContractUI->title = title;
	queryContractUI->titleLength = titleLength;	
	queryContractUI->msg = msg;
	queryContractUI->msgLength = msgLength;
}

int eth_plugin_perform_init(uint8_t *contractAddress, ethPluginInitContract_t *init) {
	uint8_t i;
	dataContext.tokenContext.pluginAvailable = 0;
	// Handle hardcoded plugin list
	PRINTF("Selector %.*H\n", 4, init->selector);
	for (i=0; i<NUM_INTERNAL_PLUGINS; i++) {
		const uint8_t **selectors = PIC(INTERNAL_ETH_PLUGINS[i].selectors);
		uint8_t j;
		for (j=0; ((j<INTERNAL_ETH_PLUGINS[i].num_selectors) && (contractAddress != NULL)); j++) {			
			if (memcmp(init->selector, PIC(selectors[j]), SELECTOR_SIZE) == 0) {
				strcpy(dataContext.tokenContext.pluginName, INTERNAL_ETH_PLUGINS[i].alias);
				dataContext.tokenContext.pluginAvailable = 1;
				contractAddress = NULL;			
				break;
			}
		}
	}

#ifndef TARGET_BLUE

	// Do not handle a plugin if running in swap mode
	if (called_from_swap && (contractAddress != NULL)) {
		PRINTF("eth_plug_init aborted in swap mode\n");
		return 0;
	}
	for (;;) {
		PRINTF("eth_plugin_init\n");
		if (contractAddress != NULL) {
			PRINTF("Trying address %.*H\n", 20, contractAddress);
		}
		else {
			PRINTF("Trying alias %s\n", dataContext.tokenContext.pluginName);
		}
		int status = eth_plugin_call(contractAddress, ETH_PLUGIN_INIT_CONTRACT, (void*)init);
		if (!status) {
			return 0;
		}
		if (status == ETH_PLUGIN_RESULT_OK) {
			break;
		}
		if (status == ETH_PLUGIN_RESULT_OK_ALIAS) {
			contractAddress = NULL;
		}
	}
	PRINTF("eth_plugin_init ok %s\n", dataContext.tokenContext.pluginName);
	dataContext.tokenContext.pluginAvailable = 1;
	return 1;

#else

	// Disable plugins on Ledger Blue for the time being

	return 0;
#endif	

}

int eth_plugin_call(uint8_t *contractAddress, int method, void *parameter) {
	ethPluginSharedRW_t pluginRW;
	ethPluginSharedRO_t pluginRO;
	char tmp[PLUGIN_ID_LENGTH];
	char *alias;
	uint8_t i;

	pluginRW.sha3 = &global_sha3;
	pluginRO.txContent = &tmpContent.txContent;

	if (contractAddress == NULL) {
		if (!dataContext.tokenContext.pluginAvailable) {
			PRINTF("Cached plugin call but no plugin available\n");
			return 0;
		}
		alias = dataContext.tokenContext.pluginName;
	}
	else {
		Base64encode(tmp, (char*)contractAddress, 20);
		alias = tmp;
	}

	// Prepare the call 

	switch(method) {
		case ETH_PLUGIN_INIT_CONTRACT:			
			((ethPluginInitContract_t*)parameter)->pluginSharedRW = &pluginRW;
			((ethPluginInitContract_t*)parameter)->pluginSharedRO = &pluginRO;
			((ethPluginInitContract_t*)parameter)->pluginContext = (uint8_t*)&dataContext.tokenContext.pluginContext;
			((ethPluginInitContract_t*)parameter)->pluginContextLength = sizeof(dataContext.tokenContext.pluginContext);
			((ethPluginInitContract_t*)parameter)->alias = dataContext.tokenContext.pluginName;
			break;
		case ETH_PLUGIN_PROVIDE_PARAMETER:
			((ethPluginProvideParameter_t*)parameter)->pluginSharedRW = &pluginRW;
			((ethPluginProvideParameter_t*)parameter)->pluginSharedRO = &pluginRO;
			((ethPluginProvideParameter_t*)parameter)->pluginContext = (uint8_t*)&dataContext.tokenContext.pluginContext;
			break;
		case ETH_PLUGIN_FINALIZE:
			((ethPluginFinalize_t*)parameter)->pluginSharedRW = &pluginRW;
			((ethPluginFinalize_t*)parameter)->pluginSharedRO = &pluginRO;
			((ethPluginFinalize_t*)parameter)->pluginContext = (uint8_t*)&dataContext.tokenContext.pluginContext;
			break;
		case ETH_PLUGIN_PROVIDE_TOKEN:
			((ethPluginProvideToken_t*)parameter)->pluginSharedRW = &pluginRW;
			((ethPluginProvideToken_t*)parameter)->pluginSharedRO = &pluginRO;
			((ethPluginProvideToken_t*)parameter)->pluginContext = (uint8_t*)&dataContext.tokenContext.pluginContext;
			break;
		case ETH_PLUGIN_QUERY_CONTRACT_ID:
			((ethQueryContractID_t*)parameter)->pluginSharedRW = &pluginRW;
			((ethQueryContractID_t*)parameter)->pluginSharedRO = &pluginRO;
			((ethQueryContractID_t*)parameter)->pluginContext = (uint8_t*)&dataContext.tokenContext.pluginContext;
			break;
		case ETH_PLUGIN_QUERY_CONTRACT_UI:
			((ethQueryContractUI_t*)parameter)->pluginSharedRW = &pluginRW;
			((ethQueryContractUI_t*)parameter)->pluginSharedRO = &pluginRO;
			((ethQueryContractUI_t*)parameter)->pluginContext = (uint8_t*)&dataContext.tokenContext.pluginContext;		
			break;
		default:
			PRINTF("Unknown plugin method %d\n", method);
			return 0;
	}

	// Perform the call

	for (i=0; i<NUM_INTERNAL_PLUGINS; i++) {
		if (strcmp(alias, INTERNAL_ETH_PLUGINS[i].alias) == 0) {
			((PluginCall)PIC(INTERNAL_ETH_PLUGINS[i].impl))(method, parameter);
			break;
		}
	}

	if (i == NUM_INTERNAL_PLUGINS) {
		 uint32_t params[3];
		 params[0] = (uint32_t)alias;
		 params[1] = method;
		 params[2] = (uint32_t)parameter;
		 BEGIN_TRY {
		 	TRY {
		 		os_lib_call(params);
		 	}
		 	CATCH_OTHER(e) {
		 		PRINTF("Plugin call exception for %s\n", alias);
		 	}
		 	FINALLY {		 		
		 	}
		 }
		 END_TRY;		
	}

	// Check the call result

	switch(method) {
		case ETH_PLUGIN_INIT_CONTRACT:					
			switch (((ethPluginInitContract_t*)parameter)->result) {
				case ETH_PLUGIN_RESULT_OK:
					if (contractAddress != NULL) {
						strcpy(dataContext.tokenContext.pluginName, alias);
					}
					break;
				case ETH_PLUGIN_RESULT_OK_ALIAS:
					break;
				default:
					return 0;
			}
			break;
		case ETH_PLUGIN_PROVIDE_PARAMETER:
			switch (((ethPluginProvideParameter_t*)parameter)->result) {
				case ETH_PLUGIN_RESULT_OK:
				case ETH_PLUGIN_RESULT_FALLBACK:
					break;
				default:
					return 0;
			}
			break;
		case ETH_PLUGIN_FINALIZE:
			switch (((ethPluginFinalize_t*)parameter)->result) {
				case ETH_PLUGIN_RESULT_OK:
				case ETH_PLUGIN_RESULT_FALLBACK:
					break;
				default:
					return 0;
			}
			break;		
		case ETH_PLUGIN_PROVIDE_TOKEN:
			switch (((ethPluginProvideToken_t*)parameter)->result) {
				case ETH_PLUGIN_RESULT_OK:
				case ETH_PLUGIN_RESULT_FALLBACK:
					break;
				default:
					return 0;
			}
			break;		
		case ETH_PLUGIN_QUERY_CONTRACT_ID:
			if (((ethQueryContractID_t*)parameter)->result != ETH_PLUGIN_RESULT_OK) {
				return 0;
			}
			break;				
		case ETH_PLUGIN_QUERY_CONTRACT_UI:
			if (((ethQueryContractUI_t*)parameter)->result != ETH_PLUGIN_RESULT_OK) {
				return 0;
			}
			break;							
		default:
			return 0;
	}

	return 1;
}
