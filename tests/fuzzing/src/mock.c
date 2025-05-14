#include <string.h>
#include <stdlib.h>

#include "cx_errors.h"
#include "cx_sha256.h"
#include "cx_sha3.h"
#include "buffer.h"
#include "lcx_ecfp.h"
#include "mem_alloc.h"

#include "bip32_utils.h"

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
                                const uint8_t keyUsageExp,
                                uint8_t *signature,
                                const uint8_t sigLen) {
    UNUSED(tag);
    UNUSED(buffer);
    UNUSED(bufLen);
    UNUSED(PubKey);
    UNUSED(keyUsageExp);
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

uint16_t get_public_key_string(bip32_path_t *bip32,
                               uint8_t *pubKey,
                               char *address,
                               uint8_t *chainCode,
                               uint64_t chainId) {
    UNUSED(bip32);
    UNUSED(pubKey);
    UNUSED(chainCode);
    UNUSED(chainId);
    memset_s(address, 0, 10);
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

typedef unsigned char bolos_task_status_t;

void os_sched_exit(__attribute__((unused)) bolos_task_status_t exit_code) {
    return;
}

int io_send_response_buffers(const buffer_t *rdatalist, size_t count, uint16_t sw) {
    UNUSED(rdatalist);
    UNUSED(count);
    UNUSED(sw);
    return 0;
}

uint16_t io_seproxyhal_send_status(uint16_t sw, uint32_t tx, bool reset, bool idle) {
    UNUSED(sw);
    UNUSED(tx);
    UNUSED(reset);
    UNUSED(idle);
    return 0;
}

uint32_t os_pki_get_info(uint8_t *key_usage,
                         uint8_t *trusted_name,
                         size_t *trusted_name_len,
                         cx_ecfp_384_public_key_t *public_key) {
    UNUSED(key_usage);
    UNUSED(trusted_name_len);
    UNUSED(public_key);
    memcpy(trusted_name, "trusted name", sizeof("trusted name"));
    return 0;
}

void ui_tx_simulation_opt_in(bool response_expected) {
    UNUSED(response_expected);
}

void ui_error_no_7702(void) {
}

void ui_error_no_7702_whitelist(void) {
}

void ui_sign_7702_auth(void) {
}

void ui_sign_7702_revocation(void) {
}

cx_err_t cx_keccak_init_no_throw(cx_sha3_t *hash PLENGTH(sizeof(cx_sha3_t)), size_t size) {
    UNUSED(size);
    memset_s(hash, 0, sizeof(cx_sha3_t));
    return CX_OK;
}

cx_err_t bip32_derive_with_seed_get_pubkey_256(unsigned int derivation_mode,
                                               cx_curve_t curve,
                                               const uint32_t *path,
                                               size_t path_len,
                                               uint8_t raw_pubkey[static 65],
                                               uint8_t *chain_code,
                                               cx_md_t hashID,
                                               unsigned char *seed,
                                               size_t seed_len) {
    UNUSED(derivation_mode);
    UNUSED(curve);
    UNUSED(path);
    UNUSED(path_len);
    UNUSED(chain_code);
    UNUSED(hashID);
    UNUSED(seed);
    UNUSED(seed_len);
    memset(raw_pubkey, 0, 65);
    return CX_OK;
}

cx_err_t bip32_derive_with_seed_ecdsa_sign_rs_hash_256(unsigned int derivation_mode,
                                                       cx_curve_t curve,
                                                       const uint32_t *path,
                                                       size_t path_len,
                                                       uint32_t sign_mode,
                                                       cx_md_t hashID,
                                                       const uint8_t *hash,
                                                       size_t hash_len,
                                                       uint8_t sig_r[static 32],
                                                       uint8_t sig_s[static 32],
                                                       uint32_t *info,
                                                       unsigned char *seed,
                                                       size_t seed_len) {
    UNUSED(derivation_mode);
    UNUSED(curve);
    UNUSED(path);
    UNUSED(path_len);
    UNUSED(sign_mode);
    UNUSED(hashID);
    UNUSED(hash);
    UNUSED(hash_len);
    UNUSED(info);
    UNUSED(seed);
    UNUSED(seed_len);
    memset(sig_r, 0, 32);
    memset(sig_s, 0, 32);
    return CX_OK;
}

// Duplicate from main.c...
const uint8_t *parseBip32(const uint8_t *dataBuffer, uint8_t *dataLength, bip32_path_t *bip32) {
    if (*dataLength < 1) {
        PRINTF("Invalid data\n");
        return NULL;
    }

    bip32->length = *dataBuffer;

    dataBuffer++;
    (*dataLength)--;

    if (*dataLength < sizeof(uint32_t) * (bip32->length)) {
        PRINTF("Invalid data\n");
        return NULL;
    }

    if (bip32_path_read(dataBuffer, (size_t) dataLength, bip32->path, (size_t) bip32->length) ==
        false) {
        PRINTF("Invalid Path data\n");
        return NULL;
    }
    dataBuffer += bip32->length * sizeof(uint32_t);
    *dataLength -= bip32->length * sizeof(uint32_t);

    return dataBuffer;
}

mem_ctx_t mem_init(void *heap_start, size_t heap_size) {
    (void) heap_size;
    return heap_start;
}

void *mem_alloc(mem_ctx_t ctx, size_t nb_bytes) {
    (void) ctx;
    return malloc(nb_bytes);
}

void mem_free(mem_ctx_t ctx, void *ptr) {
    (void) ctx;
    free(ptr);
}
