#pragma once

#include <stdint.h>

#define COLLECTION_NAME_MAX_LEN 70

typedef struct nftInfo_t {
    uint8_t contractAddress[ADDRESS_LENGTH];  // must be first item
    char collectionName[COLLECTION_NAME_MAX_LEN + 1];
} nftInfo_t;
