#ifndef EIP712_H_
#define EIP712_H_

#include <stdint.h>

enum {
    OFFSET_CLA = 0,
    OFFSET_INS,
    OFFSET_P1,
    OFFSET_P2,
    OFFSET_LC,
    OFFSET_DATA
};

typedef enum
{
    // contract defined struct
    TYPE_CUSTOM = 0,
    // native types
    TYPE_SOL_INT,
    TYPE_SOL_UINT,
    TYPE_SOL_ADDRESS,
    TYPE_SOL_BOOL,
    TYPE_SOL_STRING,
    TYPE_SOL_BYTE,
    TYPE_SOL_BYTES_FIX,
    TYPE_SOL_BYTES_DYN,
    TYPES_COUNT
}   e_type;

typedef enum
{
    ARRAY_DYNAMIC = 0,
    ARRAY_FIXED_SIZE
}   e_array_type;

typedef enum
{
    IDX_ENUM = 0,
    IDX_STR_IDX,
    IDX_COUNT
}   t_typename_matcher_idx;

#define MIN(a,b)    ((a > b) ? b : a)
#define MAX(a,b)    ((a > b) ? a : b)

// APDUs INS
#define INS_STRUCT_DEF  0x18
#define INS_STRUCT_IMPL 0x1A

// APDUs P1
#define P1_COMPLETE 0x00
#define P1_PARTIAL  0xFF

// APDUs P2
#define P2_NAME     0x00
#define P2_ARRAY    0x0F
#define P2_FIELD    0xFF

// TypeDesc masks
#define TYPE_MASK       (0xF)
#define ARRAY_MASK      (1 << 7)
#define TYPESIZE_MASK   (1 << 6)
#define TYPENAME_ENUM   (0xF)

// Solidity typenames array mask
#define TYPENAME_MORE_TYPE  (1 << 7) // For custom typename

typedef struct
{
    uint16_t    length;
    char        *str;
}   t_string;

typedef struct
{
    uint16_t    size;
    uint8_t     *ptr;
}   t_array;

typedef struct
{
    t_string    type;
    t_string    key;
    uint8_t     bytesize;
    t_array     array_levels;
}   t_struct_field;

typedef void (*field_callback)(t_struct_field *field, uint8_t index);

#endif // EIP712_H_
