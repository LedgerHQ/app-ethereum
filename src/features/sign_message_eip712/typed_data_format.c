#include <string.h>
#include "typed_data_format.h"
#include "format.h"
#include "common_utils.h"  // uint256_to_decimal, getEthDisplayableAddress
#include "shared_context.h"
#include "network.h"
#include "utils.h"  // format_signed_int_be

// getEthDisplayableAddress() refuses to write into anything smaller
#define ADDRESS_STR_SIZE 43

// "false" plus its NUL
#define BOOL_STR_SIZE 6

// Ellipsis appended in place of the text a capped value could not fit
#define ELLIPSIS     "..."
#define ELLIPSIS_LEN 3

/**
 * Decimal digits a big-endian integer of @p length bytes can reach
 *
 * ceil(length * log10(256)) in integer arithmetic; log10(256) is 2.408 so 241/100 rounds up
 * safely. 32 bytes gives 78, the digit count of 2^256 - 1.
 */
static size_t max_decimal_digits(size_t length) {
    return ((length * 241) + 99) / 100;
}

/**
 * Cap a dynamic type's text length, which the message itself would otherwise decide
 */
static size_t capped(size_t length) {
    return (length > TD_MAX_VALUE_LENGTH) ? TD_MAX_VALUE_LENGTH : length;
}

size_t td_format_value_size(const s_struct_712_field *field, size_t length) {
    if (field == NULL) {
        return 0;
    }
    switch (field->type) {
        case TYPE_SOL_STRING:
            return capped(length) + 1;
        case TYPE_SOL_ADDRESS:
            return ADDRESS_STR_SIZE;
        case TYPE_SOL_BOOL:
            return BOOL_STR_SIZE;
        case TYPE_SOL_BYTES_FIX:
        case TYPE_SOL_BYTES_DYN:
            // "0x" then two characters per byte
            return capped(2 + (length * 2)) + 1;
        case TYPE_SOL_INT:
            // one character for the sign
            return max_decimal_digits(length) + 2;
        case TYPE_SOL_UINT:
            return max_decimal_digits(length) + 1;
        case TYPE_STRUCT:
        case TYPES_COUNT:
        default:
            return 0;
    }
}

/**
 * Copy a value that is already text, appending an ellipsis if it does not fit
 *
 * @return false if the value holds a NUL byte
 */
static bool format_str(const uint8_t *data, size_t length, char *out, size_t out_size) {
    size_t max_len = out_size - 1;
    size_t to_copy = MIN(max_len, length);

    // The value is hashed in full (length-delimited) but displayed as a C string:
    // an embedded NUL would hide the trailing bytes from the review while still
    // being signed.
    if ((length > 0) && (memchr(data, '\0', length) != NULL)) {
        PRINTF("Error: EIP-712 string with embedded NUL\n");
        return false;
    }
    if (to_copy > 0) {
        memcpy(out, data, to_copy);
    }
    out[to_copy] = '\0';
    if (to_copy < length) {
        memcpy(out + max_len - ELLIPSIS_LEN, ELLIPSIS, ELLIPSIS_LEN);
        out[max_len] = '\0';
    }
    return true;
}

/**
 * Write a value as "0x" followed by its hexadecimal representation
 */
static bool format_bytes(const uint8_t *data, size_t length, char *out, size_t out_size) {
    size_t max_len = out_size - 1;
    size_t printable;

    memcpy(out, "0x", MIN(max_len, 2));
    printable = (max_len - 2) / 2;
    if (format_hex(data, MIN(printable, length), out + 2, max_len - 1) < 0) {
        return false;
    }
    if (printable < length) {
        memcpy(out + max_len - ELLIPSIS_LEN, ELLIPSIS, ELLIPSIS_LEN);
        out[max_len] = '\0';
    }
    return true;
}

bool td_format_value(const s_struct_712_field *field,
                     const uint8_t *data,
                     size_t length,
                     char *out,
                     size_t out_size) {
    if ((field == NULL) || (out == NULL)) {
        return false;
    }
    // a value whose raw bytes were released has nothing left to format
    if ((data == NULL) && (length > 0)) {
        return false;
    }
    // a value formatted into less room than it needs would be silently truncated
    if (out_size < td_format_value_size(field, length)) {
        return false;
    }
    out[0] = '\0';

    switch (field->type) {
        case TYPE_SOL_STRING:
            return format_str(data, length, out, out_size);

        case TYPE_SOL_ADDRESS:
            // no reason for an address to be received over multiple chunks
            if (length != ADDRESS_LENGTH) {
                return false;
            }
            return getEthDisplayableAddress(data, out, out_size, g_chain_config->chain_id);

        case TYPE_SOL_BOOL: {
            const char *text;

            if (length != 1) {
                return false;
            }
            text = *data ? "true" : "false";
            // BOOL_STR_SIZE covers the longest of the two, and out_size was checked against it
            memcpy(out, text, strlen(text) + 1);
            return true;
        }

        case TYPE_SOL_BYTES_FIX:
        case TYPE_SOL_BYTES_DYN:
            return format_bytes(data, length, out, out_size);

        case TYPE_SOL_INT:
            return format_signed_int_be(data, length, field->type_size, out, out_size);

        case TYPE_SOL_UINT:
            return uint256_to_decimal(data, length, out, out_size);

        case TYPE_STRUCT:
        case TYPES_COUNT:
        default:
            return false;
    }
}
