#ifndef GTP_PARSED_VALUE_H_
#define GTP_PARSED_VALUE_H_

#include <stdint.h>

typedef struct {
    uint16_t length;
    const uint8_t *ptr;
} s_parsed_value;

#define MAX_VALUE_COLLECTION_SIZE 16

typedef struct {
    uint8_t size;
    s_parsed_value value[MAX_VALUE_COLLECTION_SIZE];
} s_parsed_value_collection;

#endif  // !GTP_PARSED_VALUE_H_
