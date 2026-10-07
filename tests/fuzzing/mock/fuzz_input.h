#pragma once
/*
 * Sequential reader over the harness input.
 *
 * Harnesses draw structure through these helpers so that a short input yields
 * short fields instead of falling off a size threshold, and so every byte a
 * harness consumes ends up as a value rather than as framing the fuzzer has to
 * rediscover.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    const uint8_t *ptr;
    size_t left;
} fuzz_cursor_t;

/** Next byte, or 0 once the input is exhausted. */
static inline uint8_t fuzz_take_u8(fuzz_cursor_t *cur) {
    if (cur->left == 0) {
        return 0;
    }
    cur->left--;
    return *(cur->ptr++);
}

/** Copies @p want bytes into @p dst, zero-filling what the input cannot cover. */
static inline size_t fuzz_take_bytes(fuzz_cursor_t *cur, uint8_t *dst, size_t want) {
    size_t got = (want < cur->left) ? want : cur->left;

    if (got > 0) {
        memcpy(dst, cur->ptr, got);
        cur->ptr += got;
        cur->left -= got;
    }
    if (want > got) {
        memset(dst + got, 0, want - got);
    }
    return got;
}

/** Draws a length in [0, cap] then fills that many bytes of @p dst. */
static inline uint8_t fuzz_take_sized(fuzz_cursor_t *cur, uint8_t *dst, uint8_t cap) {
    uint8_t len = (uint8_t) (fuzz_take_u8(cur) % (cap + 1));

    fuzz_take_bytes(cur, dst, len);
    return len;
}

/**
 * @brief Length-prefixed slice of the input, without copying.
 *
 * Draws a length byte, clamps it to what remains, and returns a pointer into
 * the input. NULL when the slice would be empty.
 */
static inline const uint8_t *fuzz_take_slice(fuzz_cursor_t *cur, uint8_t *len_out) {
    size_t len = fuzz_take_u8(cur);

    if (len > cur->left) {
        len = cur->left;
    }
    *len_out = (uint8_t) len;
    if (len == 0) {
        return NULL;
    }

    const uint8_t *slice = cur->ptr;
    cur->ptr += len;
    cur->left -= len;
    return slice;
}
