#ifndef _ERC721_PLUGIN_H_
#define _ERC721_PLUGIN_H_

#ifdef HAVE_NFT_SUPPORT

#include <stdbool.h>
#include <stdint.h>
#include "asset_info.h"
#include "eth_plugin_interface.h"

// Internal plugin for EIP 721: https://eips.ethereum.org/EIPS/eip-721

typedef enum {
    APPROVE,
    SET_APPROVAL_FOR_ALL,
    TRANSFER,
    SAFE_TRANSFER,
    SAFE_TRANSFER_DATA,
    SELECTORS_COUNT
} erc721_selector_t;

typedef enum {
    FROM,
    TO,
    DATA,
    TOKEN_ID,
    OPERATOR,
    APPROVED,
    NONE,
} erc721_selector_field;

typedef struct erc721_context_t {
    uint8_t address[ADDRESS_LENGTH];
    uint8_t tokenId[INT256_LENGTH];

    bool approved;

    erc721_selector_field next_param;
    uint8_t selectorIndex;
} erc721_context_t;

void handle_provide_parameter_721(ethPluginProvideParameter_t *parameters);
void handle_query_contract_ui_721(ethQueryContractUI_t *parameters);

#endif  // HAVE_NFT_SUPPORT

#endif  // _ERC721_PLUGIN_H_
