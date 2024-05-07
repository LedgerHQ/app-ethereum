#include "manage_asset_info.h"

void reset_known_tokens(void) {
    memset(tmpCtx.transactionContext.tokenSet, 0, MAX_ITEMS);
    tmpCtx.transactionContext.currentItemIndex = 0;
}

static extraInfo_t *get_asset_info(uint8_t index) {
    if (index >= MAX_ITEMS) {
        return NULL;
    }
    return &tmpCtx.transactionContext.extraInfo[index];
}

static bool asset_info_is_set(uint8_t index) {
    if (index >= MAX_ITEMS) {
        return false;
    }
    return tmpCtx.transactionContext.tokenSet[index] != 0;
}

extraInfo_t *get_asset_info_by_addr(const uint8_t *contractAddress) {
    // Works for ERC-20 & NFT tokens since both structs in the union have the
    // contract address aligned
    for (uint8_t i = 0; i < MAX_ITEMS; i++) {
        extraInfo_t *currentItem = &tmpCtx.transactionContext.extraInfo[i];
        if (asset_info_is_set(i) &&
            (memcmp(currentItem->token.address, contractAddress, ADDRESS_LENGTH) == 0)) {
            PRINTF("Token found at index %d\n", i);
            return currentItem;
        }
    }

    return NULL;
}

extraInfo_t *get_current_asset_info(void) {
    return get_asset_info(tmpCtx.transactionContext.currentItemIndex);
}

void validate_current_asset_info(void) {
    // mark it as set
    tmpCtx.transactionContext.tokenSet[tmpCtx.transactionContext.currentItemIndex] = 1;
    // increment index
    tmpCtx.transactionContext.currentItemIndex = (tmpCtx.transactionContext.currentItemIndex + 1) % MAX_ITEMS;
}
