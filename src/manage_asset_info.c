#include "manage_asset_info.h"
#include "token_info.h"
#include "nft_info.h"

_Static_assert(offsetof(s_token_info, address) == offsetof(tokenDefinition_t, address),
               "Layout mismatch: s_token_info.address vs tokenDefinition_t.address");
_Static_assert(offsetof(s_token_info, ticker) == offsetof(tokenDefinition_t, ticker),
               "Layout mismatch: s_token_info.ticker vs tokenDefinition_t.ticker");
_Static_assert(offsetof(s_token_info, decimals) == offsetof(tokenDefinition_t, decimals),
               "Layout mismatch: s_token_info.decimals vs tokenDefinition_t.decimals");

_Static_assert(offsetof(s_nft_info, address) == offsetof(nftInfo_t, contractAddress),
               "Layout mismatch: s_nft_info.address vs nftInfo_t.contractAddress");
_Static_assert(offsetof(s_nft_info, collection_name) == offsetof(nftInfo_t, collectionName),
               "Layout mismatch: s_nft_info.collection_name vs nftInfo_t.collectionName");

// try to get an asset like how it used to work
// but since they are not in the same array anymore, we prioritize tokens over NFTs
extraInfo_t *get_matching_asset_info(const uint64_t *chain_id, const uint8_t *address) {
    extraInfo_t *asset;

    if ((asset = (extraInfo_t *) get_matching_token_info(chain_id, address)) == NULL) {
        asset = (extraInfo_t *) get_matching_nft_info(chain_id, address);
    }
    return asset;
}

void forget_known_assets(void) {
    memset(tmpCtx.transactionContext.assetSet, false, MAX_ASSETS);
    tmpCtx.transactionContext.currentAssetIndex = 0;
    clear_token_infos();
    clear_nft_infos();
}

static extraInfo_t *get_asset_info(int index) {
    if ((index < 0) || (index >= MAX_ASSETS)) {
        return NULL;
    }
    return &tmpCtx.transactionContext.extraInfo[index];
}

static bool asset_info_is_set(int index) {
    if ((index < 0) || (index >= MAX_ASSETS)) {
        return false;
    }
    return tmpCtx.transactionContext.assetSet[index];
}

int get_asset_index_by_addr(const uint8_t *addr) {
    // Works for ERC-20 & NFT tokens since both structs in the union have the
    // contract address aligned
    for (int i = 0; i < MAX_ASSETS; i++) {
        extraInfo_t *asset = get_asset_info(i);
        if (asset_info_is_set(i) && (memcmp(asset->token.address, addr, ADDRESS_LENGTH) == 0)) {
            PRINTF("Asset found at index %d\n", i);
            return i;
        }
    }
    return -1;
}

extraInfo_t *get_asset_info_by_addr(const uint8_t *addr) {
    return get_asset_info(get_asset_index_by_addr(addr));
}

extraInfo_t *get_current_asset_info(void) {
    return get_asset_info(tmpCtx.transactionContext.currentAssetIndex);
}

void validate_current_asset_info(void) {
    // mark it as set
    tmpCtx.transactionContext.assetSet[tmpCtx.transactionContext.currentAssetIndex] = true;
    // increment index
    tmpCtx.transactionContext.currentAssetIndex =
        (tmpCtx.transactionContext.currentAssetIndex + 1) % MAX_ASSETS;
}
