#pragma once

#include "tokens.h"
#include "nft.h"

typedef union extraInfo_t {
    tokenDefinition_t token;
    nftInfo_t nft;
} extraInfo_t;
