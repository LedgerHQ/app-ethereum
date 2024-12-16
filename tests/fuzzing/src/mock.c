#include <stdlib.h>

#include "cx_errors.h"
#include "cx_sha256.h"
#include "cx_sha3.h"

cx_err_t cx_sha256_init_no_throw(cx_sha256_t *hash) {
    memset(hash, 0, sizeof(cx_sha256_t));
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

uint64_t u64_from_BE(const uint8_t *in, uint8_t size) {
    uint8_t i = 0;
    uint64_t res = 0;

    while (i < size && i < sizeof(res)) {
        res <<= 8;
        res |= in[i];
        i++;
    }

    return res;
}

bool u64_to_string(uint64_t src, char *dst, uint8_t dst_size) {
    // Copy the numbers in ASCII format.
    uint8_t i = 0;
    do {
        // Checking `i + 1` to make sure we have enough space for '\0'.
        if (i + 1 >= dst_size) {
            return false;
        }
        dst[i] = src % 10 + '0';
        src /= 10;
        i++;
    } while (src);

    // Null terminate string
    dst[i] = '\0';

    // Revert the string
    i--;
    uint8_t j = 0;
    while (j < i) {
        char tmp = dst[i];
        dst[i] = dst[j];
        dst[j] = tmp;
        i--;
        j++;
    }
    return true;
}
