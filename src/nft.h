#include "tokens.h"

// SCOTT: needs comments and/or static assert and/or review this because 12 is too small
#define COLLECTION_NAME_MAX_LEN sizeof(tokenDefinition_t) - ADDRESS_LENGTH
#define MAX_NFT                 MAX_TOKEN

typedef struct nftInfo_t {
    char nftCollectionName[COLLECTION_NAME_MAX_LEN];
    char contractAddress[ADDRESS_LENGTH];
} nftInfo_t;