#ifdef HAVE_STARKWARE

#include "shared_context.h"
#include "apdu_constants.h"
#include "ethUtils.h"
#include "common_ui.h"

void handleStarkwareProvideQuantum(uint8_t p1,
                                   __attribute__((unused)) uint8_t p2,
                                   const uint8_t *dataBuffer,
                                   uint8_t dataLength,
                                   __attribute__((unused)) unsigned int *flags,
                                   __attribute__((unused)) unsigned int *tx) {
    size_t i = 0;
    uint8_t expectedDataSize = 20 + 32;
    uint8_t addressZero = 0;
    tokenDefinition_t *currentToken = NULL;
    if (appState != APP_STATE_IDLE) {
        reset_app_context();
    }
    switch (p1) {
        case STARK_QUANTUM_LEGACY:
            break;
        case STARK_QUANTUM_ETH:
        case STARK_QUANTUM_ERC20:
        case STARK_QUANTUM_ERC721:
        case STARK_QUANTUM_MINTABLE_ERC20:
        case STARK_QUANTUM_MINTABLE_ERC721:
            expectedDataSize += 32;
            break;
        default:
            THROW(0x6B00);
    }
    if (dataLength != expectedDataSize) {
        THROW(0x6700);
    }
    if (p1 == STARK_QUANTUM_LEGACY) {
        addressZero = allzeroes((void *) dataBuffer, 20);
    }
    if ((p1 != STARK_QUANTUM_ETH) && !addressZero) {
        for (i = 0; i < MAX_ITEMS; i++) {
            currentToken = &tmpCtx.transactionContext.extraInfo[i].token;
            if (tmpCtx.transactionContext.tokenSet[i] &&
                (memcmp(currentToken->address, dataBuffer, 20) == 0)) {
                break;
            }
        }
        if (i == MAX_ITEMS) {
            PRINTF("Associated token not found\n");
            THROW(0x6A80);
        }
    } else {
        i = MAX_ITEMS;
    }
    memmove(dataContext.tokenContext.quantum, dataBuffer + 20, 32);
    if (p1 != STARK_QUANTUM_LEGACY) {
        memmove(dataContext.tokenContext.mintingBlob, dataBuffer + 20 + 32, 32);
    }
    dataContext.tokenContext.quantumIndex = i;
    dataContext.tokenContext.quantumType = p1;
    quantumSet = true;
    THROW(0x9000);
}

#endif
