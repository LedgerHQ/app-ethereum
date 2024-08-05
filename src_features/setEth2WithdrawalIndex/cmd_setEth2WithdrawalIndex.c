#ifdef HAVE_ETH2

#include "shared_context.h"
#include "apdu_constants.h"

void handleSetEth2WithdrawalIndex(uint8_t p1,
                                  uint8_t p2,
                                  const uint8_t *dataBuffer,
                                  uint16_t dataLength,
                                  __attribute__((unused)) unsigned int *flags,
                                  __attribute__((unused)) unsigned int *tx) {
    if (dataLength != 4) {
        THROW(APDU_RESPONSE_WRONG_DATA_LENGTH);
    }

    if ((p1 != 0) || (p2 != 0)) {
        THROW(APDU_RESPONSE_INVALID_P1_P2);
    }

    eth2WithdrawalIndex = U4BE(dataBuffer, 0);

    THROW(APDU_RESPONSE_OK);
}

#endif
