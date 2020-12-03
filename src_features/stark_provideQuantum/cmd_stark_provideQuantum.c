#ifdef HAVE_STARKWARE

#include "shared_context.h"
#include "apdu_constants.h"
#include "ui_flow.h"

void handleStarkwareProvideQuantum(uint8_t p1,
                                   uint8_t p2,
                                   uint8_t *dataBuffer,
                                   uint16_t dataLength,
                                   unsigned int *flags,
                                   unsigned int *tx) {
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
        addressZero = allzeroes(dataBuffer, 20);
    }
    if ((p1 != STARK_QUANTUM_ETH) && !addressZero) {
        for (i = 0; i < MAX_TOKEN; i++) {
            currentToken = &tmpCtx.transactionContext.tokens[i];
            if (tmpCtx.transactionContext.tokenSet[i] &&
                (memcmp(currentToken->address, dataBuffer, 20) == 0)) {
                break;
            }
        }
        if (i == MAX_TOKEN) {
            PRINTF("Associated token not found\n");
            THROW(0x6A80);
        }
    } else {
        i = MAX_TOKEN;
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
