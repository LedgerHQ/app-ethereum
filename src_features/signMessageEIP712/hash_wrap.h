#ifndef HASH_WRAP_H_
#define HASH_WRAP_H_

#include <stdlib.h>
#include "sha3.h"

#define CX_LAST 1

typedef sha3_context cx_sha3_t;
typedef struct {} cx_hash_t;

extern cx_sha3_t   global_sha3;

int cx_keccak_init(cx_hash_t *hash, size_t size);
int cx_hash(cx_hash_t *hash, int mode, const unsigned char *in,
            unsigned int len, unsigned char *out, unsigned int out_len);
#endif // HASH_WRAP_H_
