#pragma once
/*
 * Structure builders for the internal-plugin target.
 *
 * These own the shape production would hand a plugin -- field lengths within
 * capacity, a registered asset entry -- and never author a value: every byte
 * comes from the fixed-size header below.
 *
 * The header is fixed size on purpose. Drawing the shaping bytes sequentially
 * would move the ABI words whenever a length changed, and the fuzzer would keep
 * losing the calldata it had built; here the words always start at
 * fuzz_tail_ptr[0].
 */

#include <stdint.h>

#include "tx_content.h"
#include "plugin_utils.h"  // SELECTOR_SIZE

/** Everything that shapes the call, read from data[FUZZ_CTRL_LEN]. */
typedef struct {
    uint8_t value_len;  ///< Clamped to INT256_LENGTH.
    uint8_t value[INT256_LENGTH];
    uint8_t dest_len;              ///< Clamped to ADDRESS_LENGTH.
    uint8_t dest[ADDRESS_LENGTH];  ///< Doubles as the contract and asset address.
    uint8_t chain_len;             ///< Clamped to 8.
    uint8_t chain_id[8];
    uint8_t flags;                            ///< See ETH_PLUGIN_HDR_* below.
    uint8_t selector[SELECTOR_SIZE];          ///< Used when the plugin row has no table.
    uint8_t crosschain_hash[CX_SHA256_SIZE];  ///< Swap digest Exchange would supply.
    uint8_t swap_mode;
    uint8_t asset_name_len;  ///< Clamped to sizeof(asset_name).
    uint8_t asset_name[8];
    uint8_t asset_decimals;
} eth_plugin_header_t;

enum {
    ETH_PLUGIN_HDR_DATA_PRESENT = 1u << 0,
    /* Production only resolves a token lookup when the user provided the asset
     * info beforehand, so both outcomes are reachable states. */
    ETH_PLUGIN_HDR_REGISTER_ASSET = 1u << 1,
};

/** Fills @p out from @p hdr, every field length within its array. */
void fuzz_build_tx_content(txContent_t *out, const eth_plugin_header_t *hdr);

/** Registers a token at @p address so get_matching_asset_info() resolves it. */
void fuzz_register_token(const uint8_t *address, const eth_plugin_header_t *hdr);

/** Registers an NFT collection at @p address, same purpose as above. */
void fuzz_register_nft(const uint8_t *address, const eth_plugin_header_t *hdr);
