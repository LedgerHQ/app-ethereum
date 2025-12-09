#pragma once

#include "eth_plugin_interface.h"

#define NO_EXTRA_INFO(ctx, idx) \
    (allzeroes(&(ctx.transactionContext.extraInfo[idx]), sizeof(extraInfo_t)))

#define NO_NFT_METADATA (NO_EXTRA_INFO(tmpCtx, 0))

void eth_plugin_prepare_init(ethPluginInitContract_t *init,
                             const uint8_t *selector,
                             uint32_t data_size);
void eth_plugin_prepare_provide_parameter(ethPluginProvideParameter_t *provide_parameter,
                                          uint8_t *parameter,
                                          uint32_t parameter_offset);
void eth_plugin_prepare_finalize(ethPluginFinalize_t *finalize);
void eth_plugin_prepare_provide_info(ethPluginProvideInfo_t *provide_token);
void eth_plugin_prepare_query_contract_id(ethQueryContractID_t *query_contract_id,
                                          char *name,
                                          uint32_t name_length,
                                          char *version,
                                          uint32_t version_length);
void eth_plugin_prepare_query_contract_ui(ethQueryContractUI_t *query_contract_ui,
                                          uint8_t screen_index,
                                          char *title,
                                          uint32_t title_length,
                                          char *msg,
                                          uint32_t msg_length);

eth_plugin_result_t eth_plugin_perform_init(uint8_t *contract_address,
                                            ethPluginInitContract_t *init);
// NULL for cached address, or base contract address
eth_plugin_result_t eth_plugin_call(int method, void *parameter);
