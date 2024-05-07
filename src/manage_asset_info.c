#include "manage_asset_info.h"
#include "shared_context.h"

void forget_known_assets(void) {
    memset(tmpCtx.transactionContext.assetSet, false, MAX_ASSETS);
    tmpCtx.transactionContext.currentAssetIndex = 0;
}

static extraInfo_t *get_asset_info(uint8_t index) {
    if (index >= MAX_ASSETS) {
        return NULL;
    }
    return &tmpCtx.transactionContext.extraInfo[index];
}

static bool asset_info_is_set(uint8_t index) {
    if (index >= MAX_ASSETS) {
        return false;
    }
    return tmpCtx.transactionContext.assetSet[index];
}

extraInfo_t *get_asset_info_by_addr(const uint8_t *contractAddress) {
    // Works for ERC-20 & NFT tokens since both structs in the union have the
    // contract address aligned
    for (uint8_t i = 0; i < MAX_ASSETS; i++) {
        extraInfo_t *currentItem = get_asset_info(i);
        if (asset_info_is_set(i) &&
            (memcmp(currentItem->token.address, contractAddress, ADDRESS_LENGTH) == 0)) {
            PRINTF("Token found at index %d\n", i);
            return currentItem;
        }
    }

    return NULL;
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
