#ifndef HASH_BYTES_H_
#define HASH_BYTES_H_

#include <stdint.h>
#include "cx.h"

void hash_nbytes(const uint8_t *const bytes_ptr, size_t n, cx_hash_t *hash_ctx);
void hash_byte(uint8_t byte, cx_hash_t *hash_ctx);

#endif  // HASH_BYTES_H_
