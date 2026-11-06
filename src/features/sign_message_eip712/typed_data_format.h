#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "typed_data.h"

// Longest text a single value may be displayed as, NUL excluded. Dynamic types are sized by
// the message itself, so without a bound one field could exhaust the heap.
#define TD_MAX_VALUE_LENGTH 255

/**
 * Storage a value's text representation needs, NUL included
 *
 * Derived from the field's type and the value's byte length alone, without formatting
 * anything, so a caller can allocate before it formats. Never under-reports: a value
 * formatted into a buffer of this size is never truncated for lack of room.
 *
 * @param[in] field declaration the value conforms to
 * @param[in] length value's byte length
 * @return required buffer size, or 0 if the value has no text representation
 */
size_t td_format_value_size(const s_struct_712_field *field, size_t length);

/**
 * Format a value as the text shown to the user
 *
 * @param[in] field declaration the value conforms to
 * @param[in] data value's raw bytes
 * @param[in] length value's byte length
 * @param[out] out buffer receiving the NUL-terminated text
 * @param[in] out_size size of @p out, at least td_format_value_size()
 * @return whether the value could be formatted
 */
bool td_format_value(const s_struct_712_field *field,
                     const uint8_t *data,
                     size_t length,
                     char *out,
                     size_t out_size);
