#include <string.h>
#include <stdlib.h>

#include "cx_errors.h"
#include "cx_sha256.h"
#include "cx_sha3.h"

size_t strlcpy(char *dst, const char *src, size_t size) {
    return strncpy(dst, src, size);
}

size_t strlcat(char *dst, const char *src, size_t size) {
    return strncat(dst, src, size);
}

cx_err_t cx_sha256_init_no_throw(cx_sha256_t *hash) {
    memset(hash, 0, sizeof(cx_sha256_t));
    return CX_OK;
}

cx_err_t cx_sha3_init_no_throw(cx_sha3_t *hash PLENGTH(sizeof(cx_sha3_t)), size_t size) {
    UNUSED(size);
    memset(hash, 0, sizeof(cx_sha3_t));
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
    digest[CX_KECCAK_256_SIZE - 1] = 0;
    return CX_OK;
}

cx_err_t cx_sha256_hash_iovec(const cx_iovec_t *iovec,
                              size_t iovec_len,
                              uint8_t digest[static CX_SHA256_SIZE]) {
    UNUSED(iovec);
    UNUSED(iovec_len);
    digest[CX_SHA256_SIZE - 1] = 0;
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
    if (len > 0) buffer[len - 1] = 0;
}

uint16_t get_public_key(uint8_t *out, uint8_t outLength) {
    if (outLength > 0) out[outLength - 1] = 0;
    return 0;
}

void ui_gcs_cleanup(void) {
}

size_t cx_hash_sha256(const uint8_t *in, size_t in_len, uint8_t *out, size_t out_len) {
    if (in_len > 0 && out_len > 0) out[out_len - 1] = in[in_len - 1];
    return CX_OK;
}
