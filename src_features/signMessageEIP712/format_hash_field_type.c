#ifdef HAVE_EIP712_FULL_SUPPORT

#include "format_hash_field_type.h"
#include "mem.h"
#include "mem_utils.h"
#include "commands_712.h"
#include "hash_bytes.h"
#include "apdu_constants.h"  // APDU response codes
#include "typed_data.h"

/**
 * Format & hash a struct field typesize
 *
 * @param[in] field_ptr pointer to the struct field
 * @param[in] hash_ctx pointer to the hashing context
 * @return whether the formatting & hashing were successful or not
 */
static bool format_hash_field_type_size(const void *const field_ptr, cx_hash_t *hash_ctx) {
    uint16_t field_size;
    char *uint_str_ptr;
    uint8_t uint_str_len;

    field_size = get_struct_field_typesize(field_ptr);
    switch (struct_field_type(field_ptr)) {
        case TYPE_SOL_INT:
        case TYPE_SOL_UINT:
            field_size *= 8;  // bytes -> bits
            break;
        case TYPE_SOL_BYTES_FIX:
            break;
        default:
            // should not be in here :^)
            apdu_response_code = APDU_RESPONSE_INVALID_DATA;
            return false;
    }
    uint_str_ptr = mem_alloc_and_format_uint(field_size, &uint_str_len);
    if (uint_str_ptr == NULL) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    hash_nbytes((uint8_t *) uint_str_ptr, uint_str_len, hash_ctx);
    mem_dealloc(uint_str_len);
    return true;
}

/**
 * Format & hash a struct field array levels
 *
 * @param[in] field_ptr pointer to the struct field
 * @param[in] hash_ctx pointer to the hashing context
 * @return whether the formatting & hashing were successful or not
 */
static bool format_hash_field_type_array_levels(const void *const field_ptr, cx_hash_t *hash_ctx) {
    uint8_t array_size;
    char *uint_str_ptr;
    uint8_t uint_str_len;
    const void *lvl_ptr;
    uint8_t lvls_count;

    lvl_ptr = get_struct_field_array_lvls_array(field_ptr, &lvls_count);
    while (lvls_count-- > 0) {
        hash_byte('[', hash_ctx);

        switch (struct_field_array_depth(lvl_ptr, &array_size)) {
            case ARRAY_DYNAMIC:
                break;
            case ARRAY_FIXED_SIZE:
                if ((uint_str_ptr = mem_alloc_and_format_uint(array_size, &uint_str_len)) == NULL) {
                    apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
                    return false;
                }
                hash_nbytes((uint8_t *) uint_str_ptr, uint_str_len, hash_ctx);
                mem_dealloc(uint_str_len);
                break;
            default:
                // should not be in here :^)
                apdu_response_code = APDU_RESPONSE_INVALID_DATA;
                return false;
        }
        hash_byte(']', hash_ctx);
        lvl_ptr = get_next_struct_field_array_lvl(lvl_ptr);
    }
    return true;
}

/**
 * Format & hash a struct field type
 *
 * @param[in] field_ptr pointer to the struct field
 * @param[in] hash_ctx pointer to the hashing context
 * @return whether the formatting & hashing were successful or not
 */
bool format_hash_field_type(const void *const field_ptr, cx_hash_t *hash_ctx) {
    const char *name;
    uint8_t length;

    // field type name
    name = get_struct_field_typename(field_ptr, &length);
    hash_nbytes((uint8_t *) name, length, hash_ctx);

    // field type size
    if (struct_field_has_typesize(field_ptr)) {
        if (!format_hash_field_type_size(field_ptr, hash_ctx)) {
            return false;
        }
    }

    // field type array levels
    if (struct_field_is_array(field_ptr)) {
        if (!format_hash_field_type_array_levels(field_ptr, hash_ctx)) {
            return false;
        }
    }
    return true;
}

#endif  // HAVE_EIP712_FULL_SUPPORT
