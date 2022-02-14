#include "tokens.h"

// An `nftInfo_t` must be the same size as a `tokenDefinition_t`. This is because both will be held
// in a `extraInfo_t` which is a union of a `nftInfo_t` and a `tokenDefinition_t`. By having both
// struct the same size, we know they will be aligned, which facilitates accessing the items.

// We defined the collection name max length to be the size of a `tokenDefinition_t` and remove the
// `ADDRESS_LENGTH` which corresponds to `sizeof(contractAddress`).
#define COLLECTION_NAME_MAX_LEN sizeof(tokenDefinition_t) - ADDRESS_LENGTH

typedef struct nftInfo_t {
    char collectionName[COLLECTION_NAME_MAX_LEN];
    char contractAddress[ADDRESS_LENGTH];
} nftInfo_t;