#pragma once

#include <stdint.h>
#include "cx.h"

void hash_nbytes(const uint8_t *const bytes_ptr, size_t n, cx_hash_t *hash_ctx);
void hash_byte(uint8_t byte, cx_hash_t *hash_ctx);
bool finalize_hash(cx_hash_t *hash_ctx, uint8_t *out, size_t out_len);
