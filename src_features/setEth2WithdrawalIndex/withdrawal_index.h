#ifndef _SET_WITHDRAWAL_INDEX_H_
#define _SET_WITHDRAWAL_INDEX_H_

#include "stdint.h"

void handleSetEth2WithdrawalIndex(uint8_t p1,
                                  uint8_t p2,
                                  uint8_t *dataBuffer,
                                  uint16_t dataLength,
                                  unsigned int *flags,
                                  unsigned int *tx);

#endif  // _SET_WITHDRAWAL_INDEX_H_
