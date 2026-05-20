#include <ctype.h>
#include <string.h>
#include "utils.h"
#include "common_utils.h"
#include "uint128.h"
#include "uint256.h"
#include "format.h"
#include "read.h"

/**
 * @brief Format a big-endian signed integer as a decimal string
 *
 * Mirrors the ABI encode_int() sign convention: a value is treated as negative
 * only when it arrives at full type width and has its MSB set. Shorter values
 * are always treated as non-negative and formatted via uint256_to_decimal.
 *
 * @param[in]  data      Raw big-endian bytes of the value
 * @param[in]  length    Number of bytes in data (must be 1..type_size)
 * @param[in]  type_size Declared byte width of the integer type (e.g. 32 for int256)
 * @param[out] buf       Output string buffer
 * @param[in]  buf_size  Size of buf
 * @return true on success, false on invalid input or formatting failure
 */
bool format_signed_int_be(const uint8_t *data,
                          uint8_t length,
                          uint8_t type_size,
                          char *buf,
                          size_t buf_size) {
    uint8_t tmp[INT256_LENGTH];
    union {
        uint256_t value256;
        uint128_t value128;
        int64_t value64;
    } uv;

    if ((length == 0) || (length > type_size)) {
        return false;
    }

    // Positive if truncated or MSB not set — treat as unsigned
    if ((length != type_size) || ((data[0] & 0x80) == 0)) {
        return uint256_to_decimal(data, length, buf, buf_size);
    }

    // Negative: sign-extend with 0xFF into the smallest fitting container
    if (type_size <= sizeof(uv.value64)) {
        memset(tmp, 0xFF, sizeof(uv.value64) - length);
        memcpy(tmp + sizeof(uv.value64) - length, data, length);
        uv.value64 = (int64_t) read_u64_be(tmp, 0);
        return format_i64(buf, buf_size, uv.value64);
    } else if (type_size <= sizeof(uv.value128)) {
        memset(tmp, 0xFF, sizeof(uv.value128) - length);
        memcpy(tmp + sizeof(uv.value128) - length, data, length);
        convertUint128BE(tmp, sizeof(uv.value128), &uv.value128);
        return tostring128_signed(&uv.value128, 10, buf, buf_size);
    } else if (type_size <= sizeof(uv.value256)) {
        memset(tmp, 0xFF, sizeof(uv.value256) - length);
        memcpy(tmp + sizeof(uv.value256) - length, data, length);
        convertUint256BE(tmp, sizeof(uv.value256), &uv.value256);
        return tostring256_signed(&uv.value256, 10, buf, buf_size);
    }
    PRINTF("Error: wrong int typesize (%u bytes)\n", type_size);
    return false;
}

/**
 * @brief Shrinks or expands a buffer to fit a destination size
 *
 * This function copies data from a source buffer to a destination buffer, handling
 * both cases where the source is larger (shrinking) or smaller (expanding) than the
 * destination. When expanding, the destination is zero-padded at the beginning.
 *
 * @param[in] src Pointer to the source buffer
 * @param[in] src_size Size of the source buffer
 * @param[out] dst Pointer to the destination buffer
 * @param[in] dst_size Size of the destination buffer
 */
void buf_shrink_expand(const uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_size) {
    size_t src_off;
    size_t dst_off;

    if (dst == NULL || dst_size == 0) {
        return;
    }
    if (src == NULL || src_size == 0) {
        explicit_bzero(dst, dst_size);
        return;
    }
    if (src_size > dst_size) {
        src_off = src_size - dst_size;
        dst_off = 0;
    } else {
        src_off = 0;
        dst_off = dst_size - src_size;
        explicit_bzero(dst, dst_off);
    }
    memcpy(&dst[dst_off], &src[src_off], dst_size - dst_off);
}

/**
 * @brief Copies a string with explicit truncation indication
 *
 * This function copies a string from source to destination, adding "..." if the string
 * needs to be truncated to fit in the destination buffer. The destination is always
 * null-terminated.
 *
 * @param[in] src Pointer to the source string
 * @param[in] src_size Size of the source string (without null terminator)
 * @param[out] dst Pointer to the destination buffer
 * @param[in] dst_size Size of the destination buffer (including null terminator)
 */
void str_cpy_explicit_trunc(const char *src, size_t src_size, char *dst, size_t dst_size) {
    size_t off;
    const char trunc_marker[] = "...";

    if (dst == NULL || dst_size == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    if (src_size < dst_size) {
        memcpy(dst, src, src_size);
        dst[src_size] = '\0';
    } else if (dst_size <= sizeof(trunc_marker)) {
        if (dst_size > 0) {
            dst[0] = '\0';
        }
        return;
    } else {
        off = dst_size - sizeof(trunc_marker);
        memcpy(dst, src, off);
        memcpy(&dst[off], trunc_marker, sizeof(trunc_marker));
    }
}

/**
 * @brief Reverses a string in place
 *
 * This function reverses the characters in the provided string.
 *
 * @param[in,out] str A pointer to the string to be reversed
 * @param[in] length The length of the string to be reversed
 */
void reverseString(char *const str, uint32_t length) {
    uint32_t i, j;
    for (i = 0, j = length - 1; i < j; i++, j--) {
        char c;
        c = str[i];
        str[i] = str[j];
        str[j] = c;
    }
}
