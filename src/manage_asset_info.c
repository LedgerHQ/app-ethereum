#include "manage_asset_info.h"

void reset_known_tokens(void) {
    memset(tmpCtx.transactionContext.tokenSet, 0, MAX_ITEMS);
}

extraInfo_t *get_asset_info(const uint8_t *contractAddress) {
    // Works for ERC-20 & NFT tokens since both structs in the union have the
    // contract address aligned
    for (uint8_t i = 0; i < MAX_ITEMS; i++) {
        extraInfo_t *currentItem = &tmpCtx.transactionContext.extraInfo[i];
        if (tmpCtx.transactionContext.tokenSet[i] &&
            (memcmp(currentItem->token.address, contractAddress, ADDRESS_LENGTH) == 0)) {
            PRINTF("Token found at index %d\n", i);
            return currentItem;
        }
    }

    return NULL;
}
