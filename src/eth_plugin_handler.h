#ifndef __ETH_PLUGIN_HANDLER_H__

#include "eth_plugin_interface.h"

void eth_plugin_prepare_init(ethPluginInitContract_t *init, uint8_t *selector, uint32_t dataSize);
void eth_plugin_prepare_provide_parameter(ethPluginProvideParameter_t *provideParameter,
                                          uint8_t *parameter,
                                          uint32_t parameterOffset);
void eth_plugin_prepare_finalize(ethPluginFinalize_t *finalize);
void eth_plugin_prepare_provide_token(ethPluginProvideToken_t *provideToken,
                                      tokenDefinition_t *token1,
                                      tokenDefinition_t *token2);
void eth_plugin_prepare_query_contract_ID(ethQueryContractID_t *queryContractID,
                                          char *name,
                                          uint32_t nameLength,
                                          char *version,
                                          uint32_t versionLength);
void eth_plugin_prepare_query_contract_UI(ethQueryContractUI_t *queryContractUI,
                                          uint8_t screenIndex,
                                          char *title,
                                          uint32_t titleLength,
                                          char *msg,
                                          uint32_t msgLength);

eth_plugin_result_t eth_plugin_perform_init(uint8_t *contractAddress,
                                            ethPluginInitContract_t *init);
// NULL for cached address, or base contract address
eth_plugin_result_t eth_plugin_call(int method, void *parameter);

void plugin_ui_start(void);

#endif
