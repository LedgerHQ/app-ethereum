#include <string.h>
#include <stdlib.h>

#include "cx_errors.h"
#include "cx_sha256.h"
#include "cx_sha3.h"

/** MemorySanitizer does not wrap explicit_bzero https://github.com/google/sanitizers/issues/1507
 * which results in false positives when running MemorySanitizer.
 */
void memset_s(void *buffer, char c, size_t n) {
    if (buffer == NULL) return;

    volatile char *ptr = buffer;
    while (n--) *ptr++ = c;
}

size_t strlcpy(char *dst, const char *src, size_t size) {
    const char *s = src;
    size_t n = size;

    if (n != 0) {
        while (--n != 0) {
            if ((*dst++ = *s++) == '\0') {
                break;
            }
        }
    }

    if (n == 0) {
        if (size != 0) {
            *dst = '\0';
        }
        while (*s++)
            ;
    }

    return (s - src - 1);
}

size_t strlcat(char *dst, const char *src, size_t size) {
    char *d = dst;
    const char *s = src;
    size_t n = size;
    size_t dsize;

    while (n-- != 0 && *d != '\0') {
        d++;
    }
    dsize = d - dst;
    n = size - dsize;

    if (n == 0) {
        return (dsize + strlen(s));
    }

    while (*s != '\0') {
        if (n != 1) {
            *d++ = *s;
            n--;
        }
        s++;
    }
    *d = '\0';

    return (dsize + (s - src));
}

cx_err_t cx_sha256_init_no_throw(cx_sha256_t *hash) {
    memset_s(hash, 0, sizeof(cx_sha256_t));
    return CX_OK;
}

cx_err_t cx_sha3_init_no_throw(cx_sha3_t *hash PLENGTH(sizeof(cx_sha3_t)), size_t size) {
    UNUSED(size);
    memset_s(hash, 0, sizeof(cx_sha3_t));
    return CX_OK;
}

cx_err_t cx_hash_no_throw(cx_hash_t *hash,
                          uint32_t mode,
                          const uint8_t *in,
                          size_t len,
                          uint8_t *out,
                          size_t out_len) {
    UNUSED(hash);
    UNUSED(mode);
    memset_s(out, 0, out_len);  // let's initialize the buffer
    // if arrays are not empty, read the last element of in and write it in the last element of out
    if (len > 0 && out_len > 0) out[out_len - 1] = in[len - 1];
    return CX_OK;
}

void assert_exit(bool confirm) {
    UNUSED(confirm);
    exit(1);
}

cx_err_t cx_keccak_256_hash_iovec(const cx_iovec_t *iovec,
                                  size_t iovec_len,
                                  uint8_t digest[static CX_KECCAK_256_SIZE]) {
    UNUSED(iovec);
    UNUSED(iovec_len);
    memset_s(digest, 0, CX_SHA256_SIZE);
    return CX_OK;
}

cx_err_t cx_sha256_hash_iovec(const cx_iovec_t *iovec,
                              size_t iovec_len,
                              uint8_t digest[static CX_SHA256_SIZE]) {
    UNUSED(iovec);
    UNUSED(iovec_len);
    memset_s(digest, 0, CX_SHA256_SIZE);
    return CX_OK;
}

int check_signature_with_pubkey(const char *tag,
                                uint8_t *buffer,
                                const uint8_t bufLen,
                                const uint8_t *PubKey,
                                const uint8_t keyLen,
#ifdef HAVE_LEDGER_PKI
                                const uint8_t keyUsageExp,
#endif
                                uint8_t *signature,
                                const uint8_t sigLen) {
    UNUSED(tag);
    UNUSED(buffer);
    UNUSED(bufLen);
    UNUSED(PubKey);
#ifdef HAVE_LEDGER_PKI
    UNUSED(keyUsageExp);
#endif
    UNUSED(keyLen);
    UNUSED(signature);
    UNUSED(sigLen);
    return CX_OK;
}

void *pic(void *addr) {
    return addr;
}

cx_err_t cx_math_mult_no_throw(uint8_t *r, const uint8_t *a, const uint8_t *b, size_t len) {
    UNUSED(r);
    UNUSED(a);
    UNUSED(b);
    UNUSED(len);
    return CX_OK;
}

void cx_rng_no_throw(uint8_t *buffer, size_t len) {
    memset_s(buffer, 0, len);
}

uint16_t get_public_key(uint8_t *out, uint8_t outLength) {
    memset_s(out, 0, outLength);
    return 0;
}

void ui_gcs_cleanup(void) {
}

size_t cx_hash_sha256(const uint8_t *in, size_t in_len, uint8_t *out, size_t out_len) {
    memset_s(out, 0, out_len);  // let's initialize the buffer
    // if arrays are not empty, read the last element of in and write it in the last element of out
    if (in_len > 0 && out_len > 0) out[out_len - 1] = in[in_len - 1];
    return CX_OK;
}
