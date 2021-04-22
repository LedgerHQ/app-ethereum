#ifdef HAVE_ETH2

#include "shared_context.h"
#include "apdu_constants.h"
#include "withdrawal_index.h"

void handleSetEth2WithdrawalIndex(uint8_t p1,
                                  uint8_t p2,
                                  uint8_t *dataBuffer,
                                  uint16_t dataLength,
                                  unsigned int *flags,
                                  unsigned int *tx) {
    if (dataLength != 4) {
        THROW(0x6700);
    }

    if ((p1 != 0) || (p2 != 0)) {
        THROW(0x6B00);
    }

    eth2WithdrawalIndex = U4BE(dataBuffer, 0);
    if (eth2WithdrawalIndex > INDEX_MAX) {
        THROW(0x6A80); // scott throw this error code or create new one ?
    }

    THROW(0x9000);
}

#endif
