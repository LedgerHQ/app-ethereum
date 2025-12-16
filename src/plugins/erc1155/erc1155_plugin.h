#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "uint256.h"
#include "asset_info.h"
#include "eth_plugin_interface.h"

// Internal plugin for EIP 1155: https://eips.ethereum.org/EIPS/eip-1155

typedef struct erc1155_context_t {
    uint8_t address[ADDRESS_LENGTH];
    uint8_t tokenId[INT256_LENGTH];
    uint256_t value;

    uint16_t ids_array_len;
    uint32_t ids_offset;
    uint16_t values_array_len;
    uint32_t values_offset;
    uint16_t array_index;

    bool approved;
    uint8_t next_param;
    uint8_t selectorIndex;
} erc1155_context_t;

void handle_provide_parameter_1155(ethPluginProvideParameter_t *parameters);
void handle_query_contract_ui_1155(ethQueryContractUI_t *parameters);
void erc1155_plugin_call(eth_plugin_msg_t message, void *parameters);
