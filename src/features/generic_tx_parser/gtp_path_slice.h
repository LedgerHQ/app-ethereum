#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "buffer.h"

typedef struct {
    bool has_start;
    int16_t start;
    bool has_end;
    int16_t end;
} s_slice_args;

typedef struct {
    s_slice_args *args;
} s_path_slice_context;

// The bounds of a slice once resolved against the length of what it slices
typedef struct {
    int32_t start;  // inclusive
    int32_t end;    // exclusive
} s_slice_bounds;

bool handle_slice_struct(const buffer_t *buf, s_path_slice_context *context);

/**
 * Resolve the bounds of a slice against the length of what it slices
 *
 * A negative bound counts back from the end, and an absent one is the start or the end of what is
 * sliced. The result is not checked against the length: whether a slice may reach outside of it,
 * or select nothing, is for whoever slices to decide.
 *
 * @param[in] slice the slice
 * @param[in] length the length of what is sliced
 * @return the bounds
 */
s_slice_bounds slice_resolve(const s_slice_args *slice, uint16_t length);

/**
 * Get the bytes a slice keeps of a value
 *
 * @param[in] slice the slice
 * @param[in] length the length of the value
 * @param[out] offset the offset of the first byte kept
 * @param[out] size the number of bytes kept
 * @return whether the slice is inside the value and keeps at least one byte
 */
bool slice_bytes(const s_slice_args *slice, uint16_t length, uint16_t *offset, uint16_t *size);
