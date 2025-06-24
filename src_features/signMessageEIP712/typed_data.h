#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "list.h"

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
    e_array_type type;
    uint8_t size;
} s_struct_712_field_array_level;

typedef struct struct_712_field {
    s_flist_node _list;
    // TypeDesc
    bool type_is_array : 1;
    bool type_has_size : 1;
    e_type type : 4;
    // TypeNameLength
    // TypeName
    char *type_name;
    // TypeSize
    uint8_t type_size;
    // ArrayLevelCount
    uint8_t array_level_count;
    // ArrayLevels
    s_struct_712_field_array_level *array_levels;
    // KeyNameLength
    // KeyName
    char *key_name;
} s_struct_712_field;

typedef struct struct_712 {
    s_flist_node _list;
    char *name;
    s_struct_712_field *fields;
} s_struct_712;

const void *get_array_in_mem(const void *ptr, uint8_t *array_size);
const char *get_string_in_mem(const uint8_t *ptr, uint8_t *string_length);
const char *get_struct_field_custom_typename(const s_struct_712_field *field_ptr);
const char *get_struct_field_typename(const s_struct_712_field *ptr);
e_array_type struct_field_array_depth(const uint8_t *ptr, uint8_t *array_size);
const s_struct_712 *get_struct_list(void);
const s_struct_712 *get_structn(const char *name_ptr, uint8_t name_length);
bool set_struct_name(uint8_t length, const uint8_t *name);
bool set_struct_field(uint8_t length, const uint8_t *data);
bool typed_data_init(void);
void typed_data_deinit(void);
