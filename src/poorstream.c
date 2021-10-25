#ifdef HAVE_STARKWARE

#include "poorstream.h"

void poorstream_init(poorstream_t *stream, uint8_t *buffer) {
    memset((void *) stream, 0, sizeof(poorstream_t));
    stream->pointer = buffer;
}

void poorstream_flush(poorstream_t *stream) {
    // PRINTF("Flush\n");
    *(stream->pointer + 0) = (stream->accumulator >> 56);
    *(stream->pointer + 1) = (stream->accumulator >> 48);
    *(stream->pointer + 2) = (stream->accumulator >> 40);
    *(stream->pointer + 3) = (stream->accumulator >> 32);
    *(stream->pointer + 4) = (stream->accumulator >> 24);
    *(stream->pointer + 5) = (stream->accumulator >> 16);
    *(stream->pointer + 6) = (stream->accumulator >> 8);
    *(stream->pointer + 7) = (stream->accumulator >> 0);
}

void poorstream_write_bits(poorstream_t *stream, uint64_t bits, uint32_t num_bits) {
    stream->offset += num_bits;
    if (stream->offset < 64) {
        stream->accumulator |= (bits << (64 - stream->offset));
        // PRINTF("ACC |= << %d\n", (64 - stream->offset));
    } else {
        stream->offset -= 64;
        stream->mask = ((1 << (num_bits - stream->offset)) - 1);
        // PRINTF("Mask %lx\n", stream->mask);
        // PRINTF("Offset %d\n", stream->offset);
        stream->accumulator |= ((bits >> stream->offset) & stream->mask);
        poorstream_flush(stream);
        stream->accumulator = 0;
        stream->pointer += 8;
        if (stream->offset) {
            stream->mask = ((1 << stream->offset) - 1);
            stream->accumulator |= ((bits & stream->mask) << (64 - stream->offset));
        }
    }
}

#endif
