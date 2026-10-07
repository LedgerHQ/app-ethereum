#include "format_hash_field_type.h"
#include "app_mem_utils.h"
#include "mem_utils.h"
#include "typed_data.h"

/**
 * Format & hash a struct field typesize
 *
 * @param[in] field_ptr pointer to the struct field
 * @param[in] hash_ctx pointer to the hashing context
 * @return whether the formatting & hashing were successful or not
 */
static bool format_hash_field_type_size(const s_struct_712_field *field_ptr, cx_hash_t *hash_ctx) {
    cx_err_t ret;
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
            return false;
    }
    uint_str_ptr = mem_alloc_and_format_uint(field_size);
    if (uint_str_ptr == NULL) {
        return false;
    }
    ret = cx_hash_update((cx_hash_t *) hash_ctx, (uint8_t *) uint_str_ptr, strlen(uint_str_ptr));
    APP_MEM_FREE((void *) uint_str_ptr);
    return ret == CX_OK;
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
    cx_err_t ret;
    const char *uint_str_ptr;

    for (int i = 0; i < field_ptr->array_level_count; ++i) {
        if (cx_hash_update((cx_hash_t *) hash_ctx, (uint8_t *) "[", 1) != CX_OK) {
            return false;
        }

        switch (field_ptr->array_levels[i].type) {
            case ARRAY_DYNAMIC:
                break;
            case ARRAY_FIXED_SIZE:
                if ((uint_str_ptr = mem_alloc_and_format_uint(field_ptr->array_levels[i].size)) ==
                    NULL) {
                    return false;
                }
                ret = cx_hash_update((cx_hash_t *) hash_ctx,
                                     (uint8_t *) uint_str_ptr,
                                     strlen(uint_str_ptr));
                APP_MEM_FREE((void *) uint_str_ptr);
                if (ret != CX_OK) {
                    return false;
                }
                break;
            default:
                // should not be in here :^)
                return false;
        }
        if (cx_hash_update((cx_hash_t *) hash_ctx, (uint8_t *) "]", 1) != CX_OK) {
            return false;
        }
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
    name = td_get_struct_field_typename(field_ptr);
    if (name == NULL) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) hash_ctx, (uint8_t *) name, strlen(name)) != CX_OK) {
        return false;
    }

    // field type size
    switch (field_ptr->type) {
        case TYPE_SOL_INT:
        case TYPE_SOL_UINT:
        case TYPE_SOL_BYTES_FIX:
            if (!format_hash_field_type_size(field_ptr, hash_ctx)) {
                return false;
            }
            break;
        default:
            break;
    }

    // field type array levels
    if (field_ptr->array_level_count > 0) {
        if (!format_hash_field_type_array_levels(field_ptr, hash_ctx)) {
            return false;
        }
    }
    return true;
}
