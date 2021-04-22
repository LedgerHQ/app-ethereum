#include "stdint.h"

#define INDEX_MAX 1337 // scott

void handleSetEth2WithdrawalIndex(uint8_t p1,
                                  uint8_t p2,
                                  uint8_t *dataBuffer,
                                  uint16_t dataLength,
                                  unsigned int *flags,
                                  unsigned int *tx);