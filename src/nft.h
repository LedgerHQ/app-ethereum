#ifndef _NFT_H_
#define _NFT_H_

#define COLLECTION_NAME_MAX_LEN 70

typedef struct nftInfo_t {
    uint8_t contractAddress[ADDRESS_LENGTH];  // must be first item
    char collectionName[COLLECTION_NAME_MAX_LEN + 1];
} nftInfo_t;

#endif  // _NFT_H_
