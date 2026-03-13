#include <ctype.h>
#include <string.h>
#include "utils.h"

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
 * @brief Checks if a string contains only printable ASCII characters
 *
 * This function determines if all characters in the provided string are within the
 * range of displayable ASCII characters (from 0x20 to 0x7E).
 *
 * @param[in] str A pointer to the string to be checked
 * @param[in] len The length of the string to be checked
 */
bool is_printable(const char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (isprint((int) str[i]) == 0) {
            return false;
        }
    }
    return true;
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
