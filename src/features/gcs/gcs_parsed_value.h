#pragma once

#include <stdint.h>

typedef struct {
    const uint8_t *ptr;
    uint16_t size;    // allocated size
    uint16_t offset;  // slice
    uint16_t length;  // slice
} s_parsed_value;

#define MAX_VALUE_COLLECTION_SIZE 16

typedef struct {
    uint8_t size;
    s_parsed_value value[MAX_VALUE_COLLECTION_SIZE];
} s_parsed_value_collection;
