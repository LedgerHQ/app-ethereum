#include <string.h>
#include "eth_plugin_handler.h"
#include "eth_plugin_internal.h"
#include "plugin_utils.h"
#include "shared_context.h"
#include "network.h"
#include "cmd_setPlugin.h"

void erc20_plugin_call(int message, void *parameters);

#ifdef HAVE_ETH2
void eth2_plugin_call(int message, void *parameters);
#endif

static const uint8_t ERC20_TRANSFER_SELECTOR[SELECTOR_SIZE] = {0xa9, 0x05, 0x9c, 0xbb};
static const uint8_t ERC20_APPROVE_SELECTOR[SELECTOR_SIZE] = {0x09, 0x5e, 0xa7, 0xb3};

const uint8_t *const ERC20_SELECTORS[NUM_ERC20_SELECTORS] = {ERC20_TRANSFER_SELECTOR,
                                                             ERC20_APPROVE_SELECTOR};

#ifdef HAVE_ETH2

static const uint8_t ETH2_DEPOSIT_SELECTOR[SELECTOR_SIZE] = {0x22, 0x89, 0x51, 0x18};

const uint8_t *const ETH2_SELECTORS[NUM_ETH2_SELECTORS] = {ETH2_DEPOSIT_SELECTOR};

#endif

// All internal alias names start with 'minus'

static const internalEthPlugin_t INTERNAL_ETH_PLUGINS[] = {
    {NULL, ERC20_SELECTORS, NUM_ERC20_SELECTORS, "-erc20", erc20_plugin_call},
#ifdef HAVE_ETH2
    {NULL, ETH2_SELECTORS, NUM_ETH2_SELECTORS, "-eth2", eth2_plugin_call},
#endif
};

void eth_plugin_prepare_init(ethPluginInitContract_t *init,
                             const uint8_t *selector,
                             uint32_t data_size) {
    explicit_bzero((uint8_t *) init, sizeof(ethPluginInitContract_t));
    init->selector = selector;
    init->dataSize = data_size;
}

void eth_plugin_prepare_provide_parameter(ethPluginProvideParameter_t *provide_parameter,
                                          uint8_t *parameter,
                                          uint32_t parameter_offset,
                                          uint8_t parameter_size) {
    explicit_bzero((uint8_t *) provide_parameter, sizeof(ethPluginProvideParameter_t));
    provide_parameter->parameter = parameter;
    provide_parameter->parameterOffset = parameter_offset;
    provide_parameter->parameter_size = parameter_size;
}

void eth_plugin_prepare_finalize(ethPluginFinalize_t *finalize) {
    explicit_bzero((uint8_t *) finalize, sizeof(ethPluginFinalize_t));
}

void eth_plugin_prepare_provide_info(ethPluginProvideInfo_t *provide_token) {
    explicit_bzero((uint8_t *) provide_token, sizeof(ethPluginProvideInfo_t));
}

void eth_plugin_prepare_query_contract_id(ethQueryContractID_t *query_contract_id,
                                          char *name,
                                          uint32_t name_length,
                                          char *version,
                                          uint32_t version_length) {
    explicit_bzero((uint8_t *) query_contract_id, sizeof(ethQueryContractID_t));
    query_contract_id->name = name;
    query_contract_id->nameLength = name_length;
    query_contract_id->version = version;
    query_contract_id->versionLength = version_length;
}

void eth_plugin_prepare_query_contract_ui(ethQueryContractUI_t *query_contract_ui,
                                          uint8_t screen_index,
                                          char *title,
                                          uint32_t title_length,
                                          char *msg,
                                          uint32_t msg_length) {
    uint64_t chain_id;

    explicit_bzero((uint8_t *) query_contract_ui, sizeof(ethQueryContractUI_t));

    // If no extra information was found, set the pointer to NULL
    if (NO_EXTRA_INFO(tmpCtx, 0)) {
        query_contract_ui->item1 = NULL;
    } else {
        query_contract_ui->item1 = &tmpCtx.transactionContext.extraInfo[0];
    }

    // If no extra information was found, set the pointer to NULL
    if (NO_EXTRA_INFO(tmpCtx, 1)) {
        query_contract_ui->item2 = NULL;
    } else {
        query_contract_ui->item2 = &tmpCtx.transactionContext.extraInfo[1];
    }

    query_contract_ui->screenIndex = screen_index;
    chain_id = get_tx_chain_id();
    strlcpy(query_contract_ui->network_ticker,
            get_displayable_ticker(&chain_id, chainConfig, true),
            sizeof(query_contract_ui->network_ticker));
    query_contract_ui->title = title;
    query_contract_ui->titleLength = title_length;
    query_contract_ui->msg = msg;
    query_contract_ui->msgLength = msg_length;
}

static void eth_plugin_perform_init_default(uint8_t *contract_address,
                                            ethPluginInitContract_t *init) {
    // check if the registered external plugin matches the TX contract address / selector
    if (memcmp(contract_address,
               dataContext.tokenContext.contractAddress,
               sizeof(dataContext.tokenContext.contractAddress)) != 0) {
        PRINTF("Got contract: %.*H\n", ADDRESS_LENGTH, contract_address);
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

static bool eth_plugin_perform_init_old_internal(uint8_t *contract_address,
                                                 ethPluginInitContract_t *init) {
    int i, j;
    const uint8_t *const *selectors;

    // Search internal plugin list
    for (i = 0; i < (int) ARRAYLEN(INTERNAL_ETH_PLUGINS); i++) {
        selectors = (const uint8_t *const *) PIC(INTERNAL_ETH_PLUGINS[i].selectors);
        if (selectors == NULL) {
            break;
        }
        for (j = 0; ((j < INTERNAL_ETH_PLUGINS[i].num_selectors) && (contract_address != NULL));
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

eth_plugin_result_t eth_plugin_perform_init(uint8_t *contract_address,
                                            ethPluginInitContract_t *init) {
    dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_UNAVAILABLE;

    PRINTF("Selector %.*H\n", 4, init->selector);
    switch (pluginType) {
        case PLUGIN_TYPE_NONE:
            PRINTF("eth_plugin_perform_init_old_internal\n");
            if (eth_plugin_perform_init_old_internal(contract_address, init)) {
                pluginType = PLUGIN_TYPE_OLD_INTERNAL;
                contract_address = NULL;
            }
            break;
        case PLUGIN_TYPE_ERC1155:
        case PLUGIN_TYPE_ERC721:
        case PLUGIN_TYPE_EXTERNAL:
            PRINTF("eth_plugin_perform_init_default\n");
            eth_plugin_perform_init_default(contract_address, init);
            contract_address = NULL;
            break;
        case PLUGIN_TYPE_SWAP_WITH_CALLDATA:
            PRINTF("contractAddress == %.*H\n", 20, contract_address);
            PRINTF("Fallback on swap_with_calldata plugin\n");
            contract_address = NULL;
            dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_OK;
            break;
        default:
            PRINTF("Unsupported pluginType %d\n", pluginType);
            os_sched_exit(0);
            break;
    }

    eth_plugin_result_t status = ETH_PLUGIN_RESULT_UNAVAILABLE;

    if (contract_address != NULL) {
        PRINTF("No plugin available for %.*H\n", 20, contract_address);
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
    char *alias;
    uint8_t i;

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
            ((ethPluginInitContract_t *) parameter)->txContent = &tmpContent.txContent;
            ((ethPluginInitContract_t *) parameter)->pluginContext =
                (uint8_t *) &dataContext.tokenContext.pluginContext;
            ((ethPluginInitContract_t *) parameter)->pluginContextLength =
                sizeof(dataContext.tokenContext.pluginContext);
            ((ethPluginInitContract_t *) parameter)->bip32 = &tmpCtx.transactionContext.bip32;
            break;
        case ETH_PLUGIN_PROVIDE_PARAMETER:
            PRINTF("-- PLUGIN PROVIDE PARAMETER --\n");
            ((ethPluginProvideParameter_t *) parameter)->result = ETH_PLUGIN_RESULT_UNAVAILABLE;
            ((ethPluginProvideParameter_t *) parameter)->txContent = &tmpContent.txContent;
            ((ethPluginProvideParameter_t *) parameter)->pluginContext =
                (uint8_t *) &dataContext.tokenContext.pluginContext;
            break;
        case ETH_PLUGIN_FINALIZE:
            PRINTF("-- PLUGIN FINALIZE --\n");
            ((ethPluginFinalize_t *) parameter)->result = ETH_PLUGIN_RESULT_UNAVAILABLE;
            ((ethPluginFinalize_t *) parameter)->txContent = &tmpContent.txContent;
            ((ethPluginFinalize_t *) parameter)->pluginContext =
                (uint8_t *) &dataContext.tokenContext.pluginContext;
            break;
        case ETH_PLUGIN_PROVIDE_INFO:
            PRINTF("-- PLUGIN PROVIDE INFO --\n");
            ((ethPluginProvideInfo_t *) parameter)->result = ETH_PLUGIN_RESULT_UNAVAILABLE;
            ((ethPluginProvideInfo_t *) parameter)->txContent = &tmpContent.txContent;
            ((ethPluginProvideInfo_t *) parameter)->pluginContext =
                (uint8_t *) &dataContext.tokenContext.pluginContext;
            break;
        case ETH_PLUGIN_QUERY_CONTRACT_ID:
            PRINTF("-- PLUGIN QUERY CONTRACT ID --\n");
            ((ethQueryContractID_t *) parameter)->result = ETH_PLUGIN_RESULT_UNAVAILABLE;
            ((ethQueryContractID_t *) parameter)->txContent = &tmpContent.txContent;
            ((ethQueryContractID_t *) parameter)->pluginContext =
                (uint8_t *) &dataContext.tokenContext.pluginContext;
            break;
        case ETH_PLUGIN_QUERY_CONTRACT_UI:
            PRINTF("-- PLUGIN QUERY CONTRACT UI --\n");
            ((ethQueryContractUI_t *) parameter)->txContent = &tmpContent.txContent;
            ((ethQueryContractUI_t *) parameter)->pluginContext =
                (uint8_t *) &dataContext.tokenContext.pluginContext;
            break;
        default:
            PRINTF("Unknown plugin method %d\n", method);
            return ETH_PLUGIN_RESULT_UNAVAILABLE;
    }

    switch (pluginType) {
        case PLUGIN_TYPE_EXTERNAL: {
            uint32_t params[3];
            params[0] = (uint32_t) alias;
            params[1] = method;
            params[2] = (uint32_t) parameter;
            BEGIN_TRY {
                TRY {
                    os_lib_call(params);
                }
                CATCH_OTHER(e) {
                    (void) e;
                    PRINTF("Plugin call exception for %s\n", alias);
                }
                FINALLY {
                }
            }
            END_TRY;
            break;
        }
        case PLUGIN_TYPE_SWAP_WITH_CALLDATA: {
            swap_with_calldata_plugin_call(method, parameter);
            break;
        }
        case PLUGIN_TYPE_ERC721: {
            erc721_plugin_call(method, parameter);
            break;
        }
        case PLUGIN_TYPE_ERC1155: {
            erc1155_plugin_call(method, parameter);
            break;
        }
        case PLUGIN_TYPE_OLD_INTERNAL: {
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
