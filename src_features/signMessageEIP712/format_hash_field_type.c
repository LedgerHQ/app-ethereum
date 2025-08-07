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
static bool format_hash_field_type_size(const s_struct_712_field *field_ptr, cx_hash_t *hash_ctx) {
    uint16_t field_size;
    const char *uint_str_ptr;

    field_size = field_ptr->type_size;
    switch (field_ptr->type) {
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
    uint_str_ptr = mem_alloc_and_format_uint(field_size);
    if (uint_str_ptr == NULL) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    hash_nbytes((uint8_t *) uint_str_ptr, strlen(uint_str_ptr), hash_ctx);
    app_mem_free((void *) uint_str_ptr);
    return true;
}

/**
 * Format & hash a struct field array levels
 *
 * @param[in] field_ptr pointer to the struct field
 * @param[in] hash_ctx pointer to the hashing context
 * @return whether the formatting & hashing were successful or not
 */
static bool format_hash_field_type_array_levels(const s_struct_712_field *field_ptr,
                                                cx_hash_t *hash_ctx) {
    const char *uint_str_ptr;

    for (int i = 0; i < field_ptr->array_level_count; ++i) {
        hash_byte('[', hash_ctx);

        switch (field_ptr->array_levels[i].type) {
            case ARRAY_DYNAMIC:
                break;
            case ARRAY_FIXED_SIZE:
                if ((uint_str_ptr = mem_alloc_and_format_uint(field_ptr->array_levels[i].size)) ==
                    NULL) {
                    apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
                    return false;
                }
                hash_nbytes((uint8_t *) uint_str_ptr, strlen(uint_str_ptr), hash_ctx);
                app_mem_free((void *) uint_str_ptr);
                break;
            default:
                // should not be in here :^)
                apdu_response_code = APDU_RESPONSE_INVALID_DATA;
                return false;
        }
        hash_byte(']', hash_ctx);
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
bool format_hash_field_type(const s_struct_712_field *field_ptr, cx_hash_t *hash_ctx) {
    const char *name;

    // field type name
    name = get_struct_field_typename(field_ptr);
    hash_nbytes((uint8_t *) name, strlen(name), hash_ctx);

    // field type size
    if (field_ptr->type_has_size) {
        if (!format_hash_field_type_size(field_ptr, hash_ctx)) {
            return false;
        }
    }

    // field type array levels
    if (field_ptr->type_is_array) {
        if (!format_hash_field_type_array_levels(field_ptr, hash_ctx)) {
            return false;
        }
    }
    return true;
}
