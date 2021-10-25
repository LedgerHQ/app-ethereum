#ifndef __POORSTREAM_H__
#define __POORSTREAM_H__

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "os.h"

typedef struct poorstream_t {
    uint8_t *pointer;
    uint32_t offset;
    uint64_t mask;
    uint64_t accumulator;
} poorstream_t;

void poorstream_init(poorstream_t *stream, uint8_t *buffer);
void poorstream_flush(poorstream_t *stream);
void poorstream_write_bits(poorstream_t *stream, uint64_t bits, uint32_t num_bits);

#endif
