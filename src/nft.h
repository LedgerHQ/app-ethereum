#ifndef _NFT_H_
#define _NFT_H_

#include <stdint.h>

#define COLLECTION_NAME_MAX_LEN 70
#define NO_NFT_METADATA         (NO_EXTRA_INFO(tmpCtx, 1))

typedef struct nftInfo_t {
    uint8_t contractAddress[ADDRESS_LENGTH];  // must be first item
    char collectionName[COLLECTION_NAME_MAX_LEN + 1];
} nftInfo_t;

#endif  // _NFT_H_
