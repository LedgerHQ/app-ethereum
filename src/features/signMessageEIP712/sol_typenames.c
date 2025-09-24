#include "sol_typenames.h"
#include "mem.h"
#include "mem_utils.h"
#include "os_pic.h"
#include "apdu_constants.h"  // APDU response codes
#include "typed_data.h"
#include "common_utils.h"  // ARRAY_SIZE

typedef struct {
    char *name;
    e_type value;
} s_sol_type;

static s_sol_type *g_sol_types = NULL;

/**
 * Initialize solidity typenames in memory
 *
 * @return whether the initialization went well or not
 */
bool sol_typenames_init(void) {
    uint8_t count = TYPES_COUNT - 1;  // because 0 is custom (so not solidity)

    if (g_sol_types != NULL) {
        sol_typenames_deinit();
        return false;
    }
    if ((g_sol_types = app_mem_alloc(sizeof(*g_sol_types) * count)) == NULL) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    for (int i = 0; i < count; ++i) {
        g_sol_types[i].value = i + 1;
        switch (g_sol_types[i].value) {
            case TYPE_SOL_INT:
                g_sol_types[i].name = app_mem_strdup("int");
                break;
            case TYPE_SOL_UINT:
                g_sol_types[i].name = app_mem_strdup("uint");
                break;
            case TYPE_SOL_ADDRESS:
                g_sol_types[i].name = app_mem_strdup("address");
                break;
            case TYPE_SOL_BOOL:
                g_sol_types[i].name = app_mem_strdup("bool");
                break;
            case TYPE_SOL_STRING:
                g_sol_types[i].name = app_mem_strdup("string");
                break;
            case TYPE_SOL_BYTES_FIX:
            case TYPE_SOL_BYTES_DYN:
                g_sol_types[i].name = app_mem_strdup("bytes");
                break;
            default:
                apdu_response_code = APDU_RESPONSE_INVALID_DATA;
                return false;
        }
        if (g_sol_types[i].name == NULL) {
            apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
            return false;
        }
    }
    return true;
}

void sol_typenames_deinit(void) {
    if (g_sol_types != NULL) {
        for (int i = 0; i < (TYPES_COUNT - 1); ++i) {
            app_mem_free(g_sol_types[i].name);
        }
        app_mem_free(g_sol_types);
        g_sol_types = NULL;
    }
}

/**
 * Get typename from a given field
 *
 * @param[in] field_ptr pointer to a struct field
 * @return typename or \ref NULL in case it wasn't found
 */
const char *get_struct_field_sol_typename(const s_struct_712_field *field_ptr) {
    for (int i = 0; i < (TYPES_COUNT - 1); ++i) {
        if (field_ptr->type == g_sol_types[i].value) {
            return g_sol_types[i].name;
        }
    }
    apdu_response_code = APDU_RESPONSE_INVALID_DATA;
    return NULL;  // Not found
}
