#ifndef EIP712_H_
#define EIP712_H_

#include <stdbool.h>
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

#define KECCAK256_HASH_BYTESIZE 32

#define ARRAY_SIZE(a)   (sizeof(a) / sizeof(a[0]))

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


// TODO: Move these into a new file
const char *get_struct_name(const uint8_t *ptr, uint8_t *const length);
const uint8_t *get_struct_fields_array(const uint8_t *ptr,
                                       uint8_t *const length);
const char *get_struct_field_typename(const uint8_t *ptr,
                                      uint8_t *const length);
bool    struct_field_has_typesize(const uint8_t *ptr);
uint8_t get_struct_field_typesize(const uint8_t *ptr);
bool    struct_field_is_array(const uint8_t *ptr);
e_type  struct_field_type(const uint8_t *ptr);
const uint8_t *get_struct_field_array_lvls_array(const uint8_t *ptr,
                                                 uint8_t *const length);
e_array_type struct_field_array_depth(const uint8_t *ptr,
                                      uint8_t *const array_size);
const uint8_t *get_next_struct_field_array_lvl(const uint8_t *ptr);
const char *get_struct_field_typename(const uint8_t *ptr,
                                      uint8_t *const length);
const char *get_struct_field_keyname(const uint8_t *ptr,
                                     uint8_t *const length);
const uint8_t *get_next_struct_field_array_lvl(const uint8_t *ptr);
const uint8_t *get_next_struct_field(const void *ptr);
const uint8_t *get_structn(const uint8_t *const ptr,
                           const char *const name_ptr,
                           const uint8_t name_length);
const void *get_array_in_mem(const void *ptr, uint8_t *const array_size);
const char *get_string_in_mem(const uint8_t *ptr, uint8_t *const string_length);

#endif // EIP712_H_
