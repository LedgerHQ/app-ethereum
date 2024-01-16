#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdint.h>
#include <string.h>
#include "sol_typenames.h"
#include "mem.h"
#include "os_pic.h"
#include "apdu_constants.h"  // APDU response codes
#include "typed_data.h"
#include "common_utils.h"  // ARRAY_SIZE

// Bit indicating they are more types associated to this typename
#define TYPENAME_MORE_TYPE (1 << 7)

static uint8_t *sol_typenames = NULL;

enum { IDX_ENUM = 0, IDX_STR_IDX, IDX_COUNT };

/**
 * Find a match between a typename index and all the type enums associated to it
 *
 * @param[in] enum_to_idx the type enum to typename index table
 * @param[in] t_idx typename index
 * @return whether at least one match was found
 */
static bool find_enum_matches(const uint8_t enum_to_idx[TYPES_COUNT - 1][IDX_COUNT],
                              uint8_t t_idx) {
    uint8_t *enum_match = NULL;

    // loop over enum/typename pairs
    for (uint8_t e_idx = 0; e_idx < (TYPES_COUNT - 1); ++e_idx) {
        if (t_idx == enum_to_idx[e_idx][IDX_STR_IDX])  // match
        {
            if (enum_match != NULL)  // in case of a previous match, mark it
            {
                *enum_match |= TYPENAME_MORE_TYPE;
            }
            if ((enum_match = mem_alloc(sizeof(uint8_t))) == NULL) {
                apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
                return false;
            }
            *enum_match = enum_to_idx[e_idx][IDX_ENUM];
        }
    }
    return (enum_match != NULL);
}

/**
 * Initialize solidity typenames in memory
 *
 * @return whether the initialization went well or not
 */
bool sol_typenames_init(void) {
    const char *const typenames[] = {
        "int",      // 0
        "uint",     // 1
        "address",  // 2
        "bool",     // 3
        "string",   // 4
        "bytes"     // 5
    };
    // \ref TYPES_COUNT - 1 since we don't include \ref TYPE_CUSTOM
    const uint8_t enum_to_idx[TYPES_COUNT - 1][IDX_COUNT] = {{TYPE_SOL_INT, 0},
                                                             {TYPE_SOL_UINT, 1},
                                                             {TYPE_SOL_ADDRESS, 2},
                                                             {TYPE_SOL_BOOL, 3},
                                                             {TYPE_SOL_STRING, 4},
                                                             {TYPE_SOL_BYTES_FIX, 5},
                                                             {TYPE_SOL_BYTES_DYN, 5}};
    uint8_t *typename_len_ptr;
    char *typename_ptr;

    if ((sol_typenames = mem_alloc(sizeof(uint8_t))) == NULL) {
        return false;
    }
    *(sol_typenames) = 0;
    // loop over typenames
    for (uint8_t t_idx = 0; t_idx < ARRAY_SIZE(typenames); ++t_idx) {
        // if at least one match was found
        if (find_enum_matches(enum_to_idx, t_idx)) {
            if ((typename_len_ptr = mem_alloc(sizeof(uint8_t))) == NULL) {
                apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
                return false;
            }
            // get pointer to the allocated space just above
            *typename_len_ptr = strlen(PIC(typenames[t_idx]));

            if ((typename_ptr = mem_alloc(sizeof(char) * *typename_len_ptr)) == NULL) {
                apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
                return false;
            }
            // copy typename
            memcpy(typename_ptr, PIC(typenames[t_idx]), *typename_len_ptr);
        }
        // increment array size
        *(sol_typenames) += 1;
    }
    return true;
}

/**
 * Get typename from a given field
 *
 * @param[in] field_ptr pointer to a struct field
 * @param[out] length length of the returned typename
 * @return typename or \ref NULL in case it wasn't found
 */
const char *get_struct_field_sol_typename(const uint8_t *field_ptr, uint8_t *const length) {
    e_type field_type;
    const uint8_t *typename_ptr;
    uint8_t typenames_count;
    bool more_type;
    bool typename_found;

    field_type = struct_field_type(field_ptr);
    typename_ptr = get_array_in_mem(sol_typenames, &typenames_count);
    typename_found = false;
    while (typenames_count-- > 0) {
        more_type = true;
        while (more_type) {
            more_type = *typename_ptr & TYPENAME_MORE_TYPE;
            e_type type_enum = *typename_ptr & TYPENAME_ENUM;
            if (type_enum == field_type) {
                typename_found = true;
            }
            typename_ptr += 1;
        }
        typename_ptr = (uint8_t *) get_string_in_mem(typename_ptr, length);
        if (typename_found) return (char *) typename_ptr;
        typename_ptr += *length;
    }
    apdu_response_code = APDU_RESPONSE_INVALID_DATA;
    return NULL;  // Not found
}

#endif  // HAVE_EIP712_FULL_SUPPORT
