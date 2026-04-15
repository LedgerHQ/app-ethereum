#include "rlp_encode.h"

#define RLP_NUMBER8_MAX 0x7F
#define RLP_STRING_BASE 0x80
#define RLP_LIST_BASE   0xC0
#define RLP_LIST_MAX    55

uint8_t rlpEncodeNumber(const uint8_t *number,
                        uint8_t numberLength,
                        uint8_t *output,
                        size_t output_size) {
    if (output_size < numberLength + 2) {
        return 0;
    }
    if (numberLength == 1) {
        if (number[0] == 0) {
            output[0] = RLP_STRING_BASE;
            return 1;
        }
        if (number[0] <= RLP_NUMBER8_MAX) {
            output[0] = number[0];
            return 1;
        } else {
            output[0] = RLP_STRING_BASE + 1;
            output[1] = number[0];
            return 2;
        }
    } else {
        output[0] = RLP_STRING_BASE + numberLength;
        memmove(output + 1, number, numberLength);
        return numberLength + 1;
    }
}

uint8_t rlpGetEncodedNumberLength(const uint8_t *number, uint8_t numberLength) {
    if (numberLength == 1) {
        if (number[0] <= RLP_NUMBER8_MAX) {
            return 1;
        }
    }
    return numberLength + 1;
}

uint8_t rlpGetSmallestNumber64EncodingSize(uint64_t number) {
    uint8_t size = 1;
    while (number) {
        number >>= 8;
        if (number == 0) {
            return size;
        }
        size++;
    }
    return 0;
}

uint8_t rlpGetEncodedNumber64Length(uint64_t number) {
    uint8_t size = rlpGetSmallestNumber64EncodingSize(number);
    if (size == 1) {
        if (number <= RLP_NUMBER8_MAX) {
            return 1;
        }
    }
    return size + 1;
}

uint8_t rlpEncodeListHeader8(uint8_t size, uint8_t *output, size_t output_size) {
    if (output_size < 2) {
        return 0;
    }
    if (size <= RLP_LIST_MAX) {
        output[0] = RLP_LIST_BASE + size;
        return 1;
    } else {
        output[0] = RLP_LIST_BASE + RLP_LIST_MAX + 1;
        output[1] = size;
        return 2;
    }
}
