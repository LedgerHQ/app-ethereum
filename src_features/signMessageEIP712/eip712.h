#ifndef EIP712_H_
#define EIP712_H_

#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdbool.h>
#include <stdint.h>

// APDUs INS
#define INS_STRUCT_DEF  0x18
#define INS_STRUCT_IMPL 0x1A

// APDUs P1
#define P1_COMPLETE 0x00
#define P1_PARTIAL  0xFF
#define P1_ACTIVATE         0x00
#define P1_CONTRACT_NAME    0x0F
#define P1_FIELD_NAME       0xFF

// APDUs P2
#define P2_NAME     0x00
#define P2_ARRAY    0x0F
#define P2_FIELD    0xFF
#define P2_KEY      0x00
#define P2_VALUE    0xFF

typedef enum
{
    EIP712_TYPE_HASH,
    EIP712_FIELD_HASH,
    EIP712_STRUCT_HASH
}   e_eip712_hash_type;

#define ARRAY_SIZE(a)   (sizeof(a) / sizeof(a[0]))

#define DOMAIN_STRUCT_NAME          "EIP712Domain"

#endif // HAVE_EIP712_FULL_SUPPORT

#endif // EIP712_H_
