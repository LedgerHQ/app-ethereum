#include "manage_asset_info.h"
#include "token_info.h"
#include "nft_info.h"
#include "shared_context.h"  // pluginType

_Static_assert((offsetof(s_token_info, address) == offsetof(tokenDefinition_t, address)) &&
                   (sizeof(((s_token_info *) NULL)->address) ==
                    sizeof(((tokenDefinition_t *) NULL)->address)),
               "Layout mismatch: s_token_info.address vs tokenDefinition_t.address");
_Static_assert((offsetof(s_token_info, ticker) == offsetof(tokenDefinition_t, ticker)) &&
                   (sizeof(((s_token_info *) NULL)->ticker) ==
                    sizeof(((tokenDefinition_t *) NULL)->ticker)),
               "Layout mismatch: s_token_info.ticker vs tokenDefinition_t.ticker");
_Static_assert((offsetof(s_token_info, decimals) == offsetof(tokenDefinition_t, decimals)) &&
                   (sizeof(((s_token_info *) NULL)->decimals) ==
                    sizeof(((tokenDefinition_t *) NULL)->decimals)),
               "Layout mismatch: s_token_info.decimals vs tokenDefinition_t.decimals");

_Static_assert((offsetof(s_nft_info, address) == offsetof(nftInfo_t, contractAddress)) &&
                   (sizeof(((s_nft_info *) NULL)->address) ==
                    sizeof(((nftInfo_t *) NULL)->contractAddress)),
               "Layout mismatch: s_nft_info.address vs nftInfo_t.contractAddress");
_Static_assert((offsetof(s_nft_info, collection_name) == offsetof(nftInfo_t, collectionName)) &&
                   (sizeof(((s_nft_info *) NULL)->collection_name) ==
                    sizeof(((nftInfo_t *) NULL)->collectionName)),
               "Layout mismatch: s_nft_info.collection_name vs nftInfo_t.collectionName");

// Resolve an address to the metadata the active plugin will actually read.
//
// extraInfo_t is an untagged union, so a plugin dereferences the arm its own
// semantics expect with nothing to check it against: handing an NFT record to a
// token plugin makes it read the collection name as a ticker and that name's
// 52nd byte as a decimals count, and vice versa. Restrict the lookup to the type
// the plugin expects wherever that type is known.
//
// An external plugin declares no asset type, so it keeps the historical
// token-then-NFT order; binding that case too would require the union to carry
// its own kind, which is a plugin SDK change. Without a plugin reading them,
// no asset is resolved.
extraInfo_t *get_matching_asset_info(const uint64_t *chain_id, const uint8_t *address) {
    extraInfo_t *asset;

    switch (pluginType) {
        case PLUGIN_TYPE_ERC721:
        case PLUGIN_TYPE_ERC1155:
            return (extraInfo_t *) get_matching_nft_info(chain_id, address);
        case PLUGIN_TYPE_OLD_INTERNAL:
            return (extraInfo_t *) get_matching_token_info(chain_id, address);
        case PLUGIN_TYPE_NONE:
        case PLUGIN_TYPE_SWAP_WITH_CALLDATA:
            return NULL;
        case PLUGIN_TYPE_EXTERNAL:
            break;
        default:
            PRINTF("Unexpected plugin type %d for an asset lookup\n", pluginType);
            return NULL;
    }

    if ((asset = (extraInfo_t *) get_matching_token_info(chain_id, address)) == NULL) {
        asset = (extraInfo_t *) get_matching_nft_info(chain_id, address);
    }
    return asset;
}

void forget_known_assets(void) {
    clear_token_infos();
    clear_nft_infos();
}
