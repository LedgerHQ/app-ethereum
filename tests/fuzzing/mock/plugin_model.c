#include "plugin_model.h"

#include <string.h>

#include "network.h"     // get_tx_chain_id()
#include "token_info.h"  // set_token_info()
#include "nft_info.h"    // set_nft_info()

/** Copies at most @p cap bytes, reporting how many landed in @p dst. */
static uint8_t fill(uint8_t *dst, const uint8_t *src, uint8_t raw_len, uint8_t cap) {
    uint8_t len = (uint8_t) (raw_len % (cap + 1));

    memcpy(dst, src, len);
    return len;
}

void fuzz_build_tx_content(txContent_t *out, const eth_plugin_header_t *hdr) {
    memset(out, 0, sizeof(*out));

    // gasprice, startgas and nonce stay zero: no plugin branches on them.
    out->value.length = fill(out->value.value, hdr->value, hdr->value_len, INT256_LENGTH);
    out->chainID.length = fill(out->chainID.value, hdr->chain_id, hdr->chain_len, 8);
    out->destinationLength = fill(out->destination, hdr->dest, hdr->dest_len, ADDRESS_LENGTH);
    out->dataPresent = (hdr->flags & ETH_PLUGIN_HDR_DATA_PRESENT) != 0;
}

void fuzz_register_token(const uint8_t *address, const eth_plugin_header_t *hdr) {
    s_token_info info = {0};

    memcpy(info.address, address, ADDRESS_LENGTH);
    fill((uint8_t *) info.ticker, hdr->asset_name, hdr->asset_name_len, sizeof(hdr->asset_name));
    info.decimals = hdr->asset_decimals;
    info.chain_id = get_tx_chain_id();
    set_token_info(&info);
}

void fuzz_register_nft(const uint8_t *address, const eth_plugin_header_t *hdr) {
    s_nft_info info = {0};

    memcpy(info.address, address, ADDRESS_LENGTH);
    fill((uint8_t *) info.collection_name,
         hdr->asset_name,
         hdr->asset_name_len,
         sizeof(hdr->asset_name));
    info.chain_id = get_tx_chain_id();
    set_nft_info(&info);
}
