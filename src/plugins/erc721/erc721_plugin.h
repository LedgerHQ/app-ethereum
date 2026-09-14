#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "asset_info.h"
#include "eth_plugin_interface.h"

// Internal plugin for EIP 721: https://eips.ethereum.org/EIPS/eip-721

typedef struct erc721_context_t {
    uint8_t address[ADDRESS_LENGTH];
    uint8_t tokenId[INT256_LENGTH];

    // Calldata `from`: the account being debited, not necessarily the signer
    uint8_t ownerAddress[ADDRESS_LENGTH];

    // safeTransferFrom(...,bytes) receiver payload; forwarded to the
    // recipient's onERC721Received hook, so its presence must be shown
    uint32_t data_offset;
    uint32_t data_length;

    bool approved;

    uint8_t next_param;
    uint8_t selectorIndex;
} erc721_context_t;

void handle_provide_parameter_721(ethPluginProvideParameter_t *parameters);
void handle_query_contract_ui_721(ethQueryContractUI_t *parameters);
void erc721_plugin_call(eth_plugin_msg_t message, void *parameters);
