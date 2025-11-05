#include <ctype.h>
#include <string.h>
#include "utils.h"

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

void str_cpy_explicit_trunc(const char *src, size_t src_size, char *dst, size_t dst_size) {
    size_t off;
    const char trunc_marker[] = "...";

    if (src_size < dst_size) {
        memcpy(dst, src, src_size);
        dst[src_size] = '\0';
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
