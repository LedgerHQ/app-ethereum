#include <stdint.h>
#include <string.h>
#include "hash_wrap.h"

cx_sha3_t   global_sha3;

int cx_keccak_init(cx_hash_t *hash, size_t size)
{
    sha3_context    *ctx = (sha3_context*)hash;

    (void)size;
    sha3_Init256(ctx);
    sha3_SetFlags(ctx, SHA3_FLAGS_KECCAK);
    return 0;
}

int cx_hash(cx_hash_t *hash, int mode, const unsigned char *in,
            unsigned int len, unsigned char *out, unsigned int out_len)
{
    sha3_context    *ctx = (sha3_context*)hash;
    const uint8_t   *result;

    sha3_Update(ctx, in, len);
    if (mode == CX_LAST)
    {
        result = sha3_Finalize(ctx);
        if (out != NULL)
        {
            memmove(out, result, out_len);
        }
    }
    return out_len;
}
