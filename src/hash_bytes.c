#include "hash_bytes.h"

/**
 * Continue given progressive hash on given bytes
 *
 * @param[in] bytes_ptr pointer to bytes
 * @param[in] n number of bytes to hash
 * @param[in] hash_ctx pointer to the hashing context
 */
void hash_nbytes(const uint8_t *bytes_ptr, size_t n, cx_hash_t *hash_ctx) {
    CX_ASSERT(cx_hash_no_throw(hash_ctx, 0, bytes_ptr, n, NULL, 0));
}

/**
 * Continue given progressive hash on given byte
 *
 * @param[in] byte byte to hash
 * @param[in] hash_ctx pointer to the hashing context
 */
void hash_byte(uint8_t byte, cx_hash_t *hash_ctx) {
    hash_nbytes(&byte, 1, hash_ctx);
}
