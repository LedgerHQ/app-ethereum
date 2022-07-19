#ifndef TYPED_DATA_H_
#define TYPED_DATA_H_

#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdint.h>
#include <stdbool.h>

// TypeDesc masks
#define TYPE_MASK     (0xF)
#define ARRAY_MASK    (1 << 7)
#define TYPESIZE_MASK (1 << 6)
#define TYPENAME_ENUM (0xF)

typedef enum { ARRAY_DYNAMIC = 0, ARRAY_FIXED_SIZE, ARRAY_TYPES_COUNT } e_array_type;

typedef enum {
    // contract defined struct
    TYPE_CUSTOM = 0,
    // native types
    TYPE_SOL_INT,
    TYPE_SOL_UINT,
    TYPE_SOL_ADDRESS,
    TYPE_SOL_BOOL,
    TYPE_SOL_STRING,
    TYPE_SOL_BYTES_FIX,
    TYPE_SOL_BYTES_DYN,
    TYPES_COUNT
} e_type;

typedef struct {
    uint8_t *structs_array;
    uint8_t *current_struct_fields_array;
} s_typed_data;

typedef uint8_t typedesc_t;
typedef uint8_t typesize_t;

const void *get_array_in_mem(const void *ptr, uint8_t *const array_size);
const char *get_string_in_mem(const uint8_t *ptr, uint8_t *const string_length);
bool struct_field_is_array(const uint8_t *ptr);
bool struct_field_has_typesize(const uint8_t *ptr);
e_type struct_field_type(const uint8_t *ptr);
uint8_t get_struct_field_typesize(const uint8_t *ptr);
const char *get_struct_field_custom_typename(const uint8_t *ptr, uint8_t *const length);
const char *get_struct_field_typename(const uint8_t *ptr, uint8_t *const length);
e_array_type struct_field_array_depth(const uint8_t *ptr, uint8_t *const array_size);
const uint8_t *get_next_struct_field_array_lvl(const uint8_t *const ptr);
const uint8_t *struct_field_half_skip(const uint8_t *ptr);
const uint8_t *get_struct_field_array_lvls_array(const uint8_t *const ptr, uint8_t *const length);
const char *get_struct_field_keyname(const uint8_t *ptr, uint8_t *const length);
const uint8_t *get_next_struct_field(const void *ptr);
const char *get_struct_name(const uint8_t *ptr, uint8_t *const length);
const uint8_t *get_struct_fields_array(const uint8_t *ptr, uint8_t *const length);
const uint8_t *get_next_struct(const uint8_t *ptr);
const uint8_t *get_structs_array(uint8_t *const length);
const uint8_t *get_structn(const char *const name_ptr, const uint8_t name_length);
bool set_struct_name(uint8_t length, const uint8_t *const name);
bool set_struct_field(uint8_t length, const uint8_t *const data);
bool typed_data_init(void);
void typed_data_deinit(void);

#endif  // HAVE_EIP712_FULL_SUPPORT

#endif  // TYPED_DATA_H_
