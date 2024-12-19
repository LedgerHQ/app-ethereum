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
