#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "uint256.h"
#include "asset_info.h"
#include "eth_plugin_interface.h"

// Internal plugin for EIP 1155: https://eips.ethereum.org/EIPS/eip-1155

// Maximum number of (id, value) pairs surfaced individually during a
// safeBatchTransferFrom review. Anything beyond this is reported via the
// aggregate quantity screen plus a truncation warning so the user is told
// the on-device view is incomplete.
#define ERC1155_BATCH_DISPLAY_MAX 3
// Ensure the FINALIZE screen count (4 base + 2 per pair + 1 truncation) fits
// in numScreens (uint8_t). Increase ERC1155_BATCH_DISPLAY_MAX with care.
_Static_assert(4 + 2 * ERC1155_BATCH_DISPLAY_MAX + 1 <= 255,
               "ERC1155 batch screen count overflows numScreens (uint8_t)");

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

    // SAFE_BATCH_TRANSFER review data. Without these, the UI only displayed
    // an aggregate "<total> from <count> NFT IDs" line and gave the user no
    // way to detect a high-value token ID hidden among innocuous ones.
    uint8_t batch_ids[ERC1155_BATCH_DISPLAY_MAX][INT256_LENGTH];
    uint8_t batch_values[ERC1155_BATCH_DISPLAY_MAX][INT256_LENGTH];
    uint8_t batch_displayed;
    bool batch_truncated;
} erc1155_context_t;

void handle_provide_parameter_1155(ethPluginProvideParameter_t *parameters);
void handle_query_contract_ui_1155(ethQueryContractUI_t *parameters);
void erc1155_plugin_call(eth_plugin_msg_t message, void *parameters);
