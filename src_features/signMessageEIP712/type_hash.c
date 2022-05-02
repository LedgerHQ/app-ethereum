#include <stdlib.h>
#include <stdio.h>
#include "eip712.h"
#include "mem.h"
#include "encode_type.h"
#include "type_hash.h"
#include "sha3.h"

const uint8_t *type_hash(const void *const structs_array,
                         const char *const struct_name,
                         const uint8_t struct_name_length)
{
    const void *const mem_loc_bak = mem_alloc(0); // backup the memory location
    const char  *typestr;
    uint16_t    length;
    sha3_context ctx;
    const uint8_t *hash;
    uint8_t *hash_ptr;

    typestr = encode_type(structs_array, struct_name, struct_name_length, &length);
    if (typestr == NULL)
    {
        return NULL;
    }
    sha3_Init256(&ctx);
    sha3_SetFlags(&ctx, SHA3_FLAGS_KECCAK);
    sha3_Update(&ctx, typestr, length);
    hash = sha3_Finalize(&ctx);

#ifdef DEBUG
    fwrite(typestr, sizeof(char), length, stdout);
    printf("\n");
#endif

    // restore the memory location
    mem_dealloc(mem_alloc(0) - mem_loc_bak);

    // copy hash into memory
    if ((hash_ptr = mem_alloc(KECCAK256_HASH_LENGTH)) == NULL)
    {
        return NULL;
    }
#ifdef DEBUG
    printf("-> 0x");
#endif
    for (int idx = 0; idx < KECCAK256_HASH_LENGTH; ++idx)
    {
        hash_ptr[idx] = hash[idx];
#ifdef DEBUG
        printf("%.02x", hash[idx]);
#endif
    }
#ifdef DEBUG
    printf("\n");
#endif
    return hash_ptr;
}
