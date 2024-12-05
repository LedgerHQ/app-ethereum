#include <string.h>
#include "eth_plugin_handler.h"
#include "eth_plugin_internal.h"
#include "plugin_utils.h"
#include "shared_context.h"
#include "network.h"
#include "cmd_setPlugin.h"

void eth_plugin_prepare_init(ethPluginInitContract_t *init,
                             const uint8_t *selector,
                             uint32_t dataSize) {
    memset((uint8_t *) init, 0, sizeof(ethPluginInitContract_t));
    init->selector = selector;
    init->dataSize = dataSize;
}

void eth_plugin_prepare_provide_parameter(ethPluginProvideParameter_t *provideParameter,
                                          uint8_t *parameter,
                                          uint32_t parameterOffset) {
    memset((uint8_t *) provideParameter, 0, sizeof(ethPluginProvideParameter_t));
    provideParameter->parameter = parameter;
    provideParameter->parameterOffset = parameterOffset;
}

void eth_plugin_prepare_finalize(ethPluginFinalize_t *finalize) {
    memset((uint8_t *) finalize, 0, sizeof(ethPluginFinalize_t));
}

void eth_plugin_prepare_provide_info(ethPluginProvideInfo_t *provideToken) {
    memset((uint8_t *) provideToken, 0, sizeof(ethPluginProvideInfo_t));
}

void eth_plugin_prepare_query_contract_ID(ethQueryContractID_t *queryContractID,
                                          char *name,
                                          uint32_t nameLength,
                                          char *version,
                                          uint32_t versionLength) {
    memset((uint8_t *) queryContractID, 0, sizeof(ethQueryContractID_t));
    queryContractID->name = name;
    queryContractID->nameLength = nameLength;
    queryContractID->version = version;
    queryContractID->versionLength = versionLength;
}

void eth_plugin_prepare_query_contract_UI(ethQueryContractUI_t *queryContractUI,
                                          uint8_t screenIndex,
                                          char *title,
                                          uint32_t titleLength,
                                          char *msg,
                                          uint32_t msgLength) {
    uint64_t chain_id;

    memset((uint8_t *) queryContractUI, 0, sizeof(ethQueryContractUI_t));

    // If no extra information was found, set the pointer to NULL
    if (NO_EXTRA_INFO(tmpCtx, 0)) {
        queryContractUI->item1 = NULL;
    } else {
        queryContractUI->item1 = &tmpCtx.transactionContext.extraInfo[0];
    }

    // If no extra information was found, set the pointer to NULL
    if (NO_EXTRA_INFO(tmpCtx, 1)) {
        queryContractUI->item2 = NULL;
    } else {
        queryContractUI->item2 = &tmpCtx.transactionContext.extraInfo[1];
    }

    queryContractUI->screenIndex = screenIndex;
    chain_id = get_tx_chain_id();
    strlcpy(queryContractUI->network_ticker,
            get_displayable_ticker(&chain_id, chainConfig),
            sizeof(queryContractUI->network_ticker));
    queryContractUI->title = title;
    queryContractUI->titleLength = titleLength;
    queryContractUI->msg = msg;
    queryContractUI->msgLength = msgLength;
}

static void eth_plugin_perform_init_default(uint8_t *contractAddress,
                                            ethPluginInitContract_t *init) {
    // check if the registered external plugin matches the TX contract address / selector
    if (memcmp(contractAddress,
               dataContext.tokenContext.contractAddress,
               sizeof(dataContext.tokenContext.contractAddress)) != 0) {
        PRINTF("Got contract: %.*H\n", ADDRESS_LENGTH, contractAddress);
        PRINTF("Expected contract: %.*H\n",
               ADDRESS_LENGTH,
               dataContext.tokenContext.contractAddress);
        os_sched_exit(0);
    }
    if (memcmp(init->selector,
               dataContext.tokenContext.methodSelector,
               sizeof(dataContext.tokenContext.methodSelector)) != 0) {
        PRINTF("Got selector: %.*H\n", SELECTOR_SIZE, init->selector);
        PRINTF("Expected selector: %.*H\n", SELECTOR_SIZE, dataContext.tokenContext.methodSelector);
        os_sched_exit(0);
    }
    PRINTF("Plugin will be used\n");
    dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_OK;
}

static bool eth_plugin_perform_init_old_internal(uint8_t *contractAddress,
                                                 ethPluginInitContract_t *init) {
    uint8_t i, j;
    const uint8_t *const *selectors;

    // Search internal plugin list
    for (i = 0;; i++) {
        selectors = (const uint8_t *const *) PIC(INTERNAL_ETH_PLUGINS[i].selectors);
        if (selectors == NULL) {
            break;
        }
        for (j = 0; ((j < INTERNAL_ETH_PLUGINS[i].num_selectors) && (contractAddress != NULL));
             j++) {
            if (memcmp(init->selector, (const void *) PIC(selectors[j]), SELECTOR_SIZE) == 0) {
                if ((INTERNAL_ETH_PLUGINS[i].availableCheck == NULL) ||
                    ((PluginAvailableCheck) PIC(INTERNAL_ETH_PLUGINS[i].availableCheck))()) {
                    strlcpy(dataContext.tokenContext.pluginName,
                            INTERNAL_ETH_PLUGINS[i].alias,
                            PLUGIN_ID_LENGTH);
                    dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_OK;
                    return true;
                }
            }
        }
    }

    return false;
}

eth_plugin_result_t eth_plugin_perform_init(uint8_t *contractAddress,
                                            ethPluginInitContract_t *init) {
    dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_UNAVAILABLE;

    PRINTF("Selector %.*H\n", 4, init->selector);
    switch (pluginType) {
#ifdef HAVE_NFT_SUPPORT
        case ERC1155:
        case ERC721:
#endif  // HAVE_NFT_SUPPORT
        case EXTERNAL:
            PRINTF("eth_plugin_perform_init_default\n");
            eth_plugin_perform_init_default(contractAddress, init);
            contractAddress = NULL;
            break;
        case OLD_INTERNAL:
            PRINTF("eth_plugin_perform_init_old_internal\n");
            if (eth_plugin_perform_init_old_internal(contractAddress, init)) {
                contractAddress = NULL;
            }
            break;
        case SWAP_WITH_CALLDATA:
            PRINTF("contractAddress == %.*H\n", 20, contractAddress);
            PRINTF("Fallback on swap_with_calldata plugin\n");
            contractAddress = NULL;
            dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_OK;
            break;
        default:
            PRINTF("Unsupported pluginType %d\n", pluginType);
            os_sched_exit(0);
            break;
    }

    eth_plugin_result_t status = ETH_PLUGIN_RESULT_UNAVAILABLE;

    if (contractAddress != NULL) {
        PRINTF("No plugin available for %.*H\n", 20, contractAddress);
        if (G_called_from_swap) {
            PRINTF("Not supported in swap mode\n");
            return ETH_PLUGIN_RESULT_ERROR;
        } else {
            return status;
        }
    }

    PRINTF("eth_plugin_init\n");
    PRINTF("Trying plugin %s\n", dataContext.tokenContext.pluginName);
    status = eth_plugin_call(ETH_PLUGIN_INIT_CONTRACT, (void *) init);

    if (status <= ETH_PLUGIN_RESULT_UNSUCCESSFUL) {
        return status;
    }
    PRINTF("eth_plugin_init ok %s\n", dataContext.tokenContext.pluginName);
    dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_OK;
    return ETH_PLUGIN_RESULT_OK;
}

eth_plugin_result_t eth_plugin_call(int method, void *parameter) {
    ethPluginSharedRW_t pluginRW;
    ethPluginSharedRO_t pluginRO;
    char *alias;
    uint8_t i;

    pluginRW.sha3 = &global_sha3;
    pluginRO.txContent = &tmpContent.txContent;

    if (dataContext.tokenContext.pluginStatus <= ETH_PLUGIN_RESULT_UNSUCCESSFUL) {
        PRINTF("Cached plugin call but no plugin available\n");
        return dataContext.tokenContext.pluginStatus;
    }
    alias = dataContext.tokenContext.pluginName;

    // Prepare the call

    switch (method) {
        case ETH_PLUGIN_INIT_CONTRACT:
            PRINTF("-- PLUGIN INIT CONTRACT --\n");
            ((ethPluginInitContract_t *) parameter)->interfaceVersion =
                ETH_PLUGIN_INTERFACE_VERSION_LATEST;
            ((ethPluginInitContract_t *) parameter)->result = ETH_PLUGIN_RESULT_UNAVAILABLE;
            ((ethPluginInitContract_t *) parameter)->pluginSharedRW = &pluginRW;
            ((ethPluginInitContract_t *) parameter)->pluginSharedRO = &pluginRO;
            ((ethPluginInitContract_t *) parameter)->pluginContext =
                (uint8_t *) &dataContext.tokenContext.pluginContext;
            ((ethPluginInitContract_t *) parameter)->pluginContextLength =
                sizeof(dataContext.tokenContext.pluginContext);
            ((ethPluginInitContract_t *) parameter)->bip32 = &tmpCtx.transactionContext.bip32;
            break;
        case ETH_PLUGIN_PROVIDE_PARAMETER:
            PRINTF("-- PLUGIN PROVIDE PARAMETER --\n");
            ((ethPluginProvideParameter_t *) parameter)->result = ETH_PLUGIN_RESULT_UNAVAILABLE;
            ((ethPluginProvideParameter_t *) parameter)->pluginSharedRW = &pluginRW;
            ((ethPluginProvideParameter_t *) parameter)->pluginSharedRO = &pluginRO;
            ((ethPluginProvideParameter_t *) parameter)->pluginContext =
                (uint8_t *) &dataContext.tokenContext.pluginContext;
            break;
        case ETH_PLUGIN_FINALIZE:
            PRINTF("-- PLUGIN FINALIZE --\n");
            ((ethPluginFinalize_t *) parameter)->result = ETH_PLUGIN_RESULT_UNAVAILABLE;
            ((ethPluginFinalize_t *) parameter)->pluginSharedRW = &pluginRW;
            ((ethPluginFinalize_t *) parameter)->pluginSharedRO = &pluginRO;
            ((ethPluginFinalize_t *) parameter)->pluginContext =
                (uint8_t *) &dataContext.tokenContext.pluginContext;
            break;
        case ETH_PLUGIN_PROVIDE_INFO:
            PRINTF("-- PLUGIN PROVIDE INFO --\n");
            ((ethPluginProvideInfo_t *) parameter)->result = ETH_PLUGIN_RESULT_UNAVAILABLE;
            ((ethPluginProvideInfo_t *) parameter)->pluginSharedRW = &pluginRW;
            ((ethPluginProvideInfo_t *) parameter)->pluginSharedRO = &pluginRO;
            ((ethPluginProvideInfo_t *) parameter)->pluginContext =
                (uint8_t *) &dataContext.tokenContext.pluginContext;
            break;
        case ETH_PLUGIN_QUERY_CONTRACT_ID:
            PRINTF("-- PLUGIN QUERY CONTRACT ID --\n");
            ((ethQueryContractID_t *) parameter)->result = ETH_PLUGIN_RESULT_UNAVAILABLE;
            ((ethQueryContractID_t *) parameter)->pluginSharedRW = &pluginRW;
            ((ethQueryContractID_t *) parameter)->pluginSharedRO = &pluginRO;
            ((ethQueryContractID_t *) parameter)->pluginContext =
                (uint8_t *) &dataContext.tokenContext.pluginContext;
            break;
        case ETH_PLUGIN_QUERY_CONTRACT_UI:
            PRINTF("-- PLUGIN QUERY CONTRACT UI --\n");
            ((ethQueryContractUI_t *) parameter)->pluginSharedRW = &pluginRW;
            ((ethQueryContractUI_t *) parameter)->pluginSharedRO = &pluginRO;
            ((ethQueryContractUI_t *) parameter)->pluginContext =
                (uint8_t *) &dataContext.tokenContext.pluginContext;
            break;
        default:
            PRINTF("Unknown plugin method %d\n", method);
            return ETH_PLUGIN_RESULT_UNAVAILABLE;
    }

    switch (pluginType) {
        case EXTERNAL: {
            uint32_t params[3];
            params[0] = (uint32_t) alias;
            params[1] = method;
            params[2] = (uint32_t) parameter;
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
            break;
        }
        case SWAP_WITH_CALLDATA: {
            swap_with_calldata_plugin_call(method, parameter);
            break;
        }
#ifdef HAVE_NFT_SUPPORT
        case ERC721: {
            erc721_plugin_call(method, parameter);
            break;
        }
        case ERC1155: {
            erc1155_plugin_call(method, parameter);
            break;
        }
#endif  // HAVE_NFT_SUPPORT
        case OLD_INTERNAL: {
            // Perform the call
            for (i = 0;; i++) {
                if (INTERNAL_ETH_PLUGINS[i].alias[0] == 0) {
                    break;
                }
                if (strcmp(alias, INTERNAL_ETH_PLUGINS[i].alias) == 0) {
                    ((PluginCall) PIC(INTERNAL_ETH_PLUGINS[i].impl))(method, parameter);
                    break;
                }
            }
            break;
        }
        default: {
            PRINTF("Error with pluginType: %d\n", pluginType);
            return ETH_PLUGIN_RESULT_ERROR;
        }
    }

    // Check the call result
    PRINTF("method: %d\n", method);
    switch (method) {
        case ETH_PLUGIN_INIT_CONTRACT:
            switch (((ethPluginInitContract_t *) parameter)->result) {
                case ETH_PLUGIN_RESULT_OK:
                    break;
                case ETH_PLUGIN_RESULT_ERROR:
                    return ETH_PLUGIN_RESULT_ERROR;
                default:
                    return ETH_PLUGIN_RESULT_UNAVAILABLE;
            }
            break;
        case ETH_PLUGIN_PROVIDE_PARAMETER:
            switch (((ethPluginProvideParameter_t *) parameter)->result) {
                case ETH_PLUGIN_RESULT_OK:
                case ETH_PLUGIN_RESULT_FALLBACK:
                    break;
                case ETH_PLUGIN_RESULT_ERROR:
                    return ETH_PLUGIN_RESULT_ERROR;
                default:
                    return ETH_PLUGIN_RESULT_UNAVAILABLE;
            }
            break;
        case ETH_PLUGIN_FINALIZE:
            switch (((ethPluginFinalize_t *) parameter)->result) {
                case ETH_PLUGIN_RESULT_OK:
                case ETH_PLUGIN_RESULT_FALLBACK:
                    break;
                case ETH_PLUGIN_RESULT_ERROR:
                    return ETH_PLUGIN_RESULT_ERROR;
                default:
                    return ETH_PLUGIN_RESULT_UNAVAILABLE;
            }
            break;
        case ETH_PLUGIN_PROVIDE_INFO:
            PRINTF("RESULT: %d\n", ((ethPluginProvideInfo_t *) parameter)->result);
            switch (((ethPluginProvideInfo_t *) parameter)->result) {
                case ETH_PLUGIN_RESULT_OK:
                case ETH_PLUGIN_RESULT_FALLBACK:
                    break;
                case ETH_PLUGIN_RESULT_ERROR:
                    return ETH_PLUGIN_RESULT_ERROR;
                default:
                    return ETH_PLUGIN_RESULT_UNAVAILABLE;
            }
            break;
        case ETH_PLUGIN_QUERY_CONTRACT_ID:
            if (((ethQueryContractID_t *) parameter)->result != ETH_PLUGIN_RESULT_OK) {
                return ETH_PLUGIN_RESULT_ERROR;
            }
            break;
        case ETH_PLUGIN_QUERY_CONTRACT_UI:
            if (((ethQueryContractUI_t *) parameter)->result != ETH_PLUGIN_RESULT_OK) {
                return ETH_PLUGIN_RESULT_ERROR;
            }
            break;
        default:
            return ETH_PLUGIN_RESULT_UNAVAILABLE;
    }

    return ETH_PLUGIN_RESULT_OK;
}
