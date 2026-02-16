#include "typed_data.h"
#include "sol_typenames.h"
#include "apdu_constants.h"  // APDU response codes
#include "context_712.h"
#include "app_mem_utils.h"

static s_struct_712 *g_structs = NULL;

/**
 * Initialize the typed data context
 *
 * @return whether the memory allocation was successful
 */
bool typed_data_init(void) {
    if (g_structs != NULL) {
        typed_data_deinit();
        return false;
    }
    return true;
}

// to be used as a \ref f_list_node_del
static void delete_field(s_struct_712_field *f) {
    APP_MEM_FREE(f->type_name);
    APP_MEM_FREE(f->array_levels);
    APP_MEM_FREE(f->key_name);
    APP_MEM_FREE(f);
}

// to be used as a \ref f_list_node_del
static void delete_struct(s_struct_712 *s) {
    APP_MEM_FREE(s->name);
    flist_clear((flist_node_t **) &s->fields, (f_list_node_del) &delete_field);
    APP_MEM_FREE(s);
}

void typed_data_deinit(void) {
    flist_clear((flist_node_t **) &g_structs, (f_list_node_del) &delete_struct);
}

/**
 * Get type name from a struct field
 *
 * @param[in] field_ptr struct field pointer
 * @return type name pointer
 */
const char *get_struct_field_typename(const s_struct_712_field *field_ptr) {
    if (field_ptr == NULL) {
        return NULL;
    }
    if (field_ptr->type == TYPE_CUSTOM) {
        return field_ptr->type_name;
    }
    return get_struct_field_sol_typename(field_ptr);
}

const s_struct_712 *get_struct_list(void) {
    return g_structs;
}

/**
 * Find struct with a given name
 *
 * @param[in] name struct name
 * @param[in] length name length
 * @return pointer to struct
 */
const s_struct_712 *get_structn(const char *name, uint8_t length) {
    const s_struct_712 *struct_ptr;

    if (name == NULL) {
        apdu_response_code = SWO_INCORRECT_DATA;
        return NULL;
    }
    for (struct_ptr = get_struct_list(); struct_ptr != NULL;
         struct_ptr = (s_struct_712 *) ((flist_node_t *) struct_ptr)->next) {
        if (struct_ptr->name != NULL) {
            if ((length == strlen(struct_ptr->name)) &&
                (memcmp(name, struct_ptr->name, length) == 0)) {
                return struct_ptr;
            }
        }
    }
    apdu_response_code = SWO_INCORRECT_DATA;
    return NULL;
}

/**
 * Set struct name
 *
 * @param[in] length name length
 * @param[in] name name
 * @return whether it was successful
 */
bool set_struct_name(uint8_t length, const uint8_t *name) {
    s_struct_712 *new_struct;

    if (name == NULL) {
        apdu_response_code = SWO_INCORRECT_DATA;
        return false;
    }

    if (APP_MEM_CALLOC((void **) &new_struct, sizeof(*new_struct)) == false) {
        apdu_response_code = SWO_INSUFFICIENT_MEMORY;
        return false;
    }

    if ((new_struct->name = APP_MEM_ALLOC(length + 1)) == NULL) {
        apdu_response_code = SWO_INSUFFICIENT_MEMORY;
        return false;
    }
    new_struct->name[length] = '\0';
    memmove(new_struct->name, name, length);
    struct_state = INITIALIZED;

    flist_push_back((flist_node_t **) &g_structs, (flist_node_t *) new_struct);
    return true;
}

/**
 * Set struct field TypeDesc
 *
 * @param[in] data the field data
 * @param[in] data_idx the data index
 * @return whether it was successful or not
 */
static bool set_struct_field_typedesc(s_struct_712_field *field,
                                      const uint8_t *data,
                                      uint8_t *data_idx,
                                      uint8_t length) {
    uint8_t typedesc;

    // copy TypeDesc
    if ((*data_idx + sizeof(typedesc)) > length)  // check buffer bound
    {
        apdu_response_code = SWO_INCORRECT_DATA;
        return false;
    }
    typedesc = data[(*data_idx)++];
    field->type_is_array = typedesc & ARRAY_MASK;
    field->type_has_size = typedesc & TYPESIZE_MASK;
    field->type = typedesc & TYPE_MASK;
    return true;
}

/**
 * Set struct field custom typename
 *
 * @param[in] data the field data
 * @param[in] data_idx the data index
 * @return whether it was successful
 */
static bool set_struct_field_custom_typename(s_struct_712_field *field,
                                             const uint8_t *data,
                                             uint8_t *data_idx,
                                             uint8_t length) {
    uint8_t typename_len;

    // copy custom struct name length
    if ((*data_idx + sizeof(typename_len)) > length)  // check buffer bound
    {
        apdu_response_code = SWO_INCORRECT_DATA;
        return false;
    }
    typename_len = data[(*data_idx)++];

    // copy name
    if ((*data_idx + typename_len) > length)  // check buffer bound
    {
        apdu_response_code = SWO_INCORRECT_DATA;
        return false;
    }
    if ((field->type_name = APP_MEM_ALLOC(typename_len + 1)) == NULL) {
        apdu_response_code = SWO_INSUFFICIENT_MEMORY;
        return false;
    }

    field->type_name[typename_len] = '\0';
    memmove(field->type_name, &data[*data_idx], typename_len);
    *data_idx += typename_len;
    return true;
}

/**
 * Set struct field's array levels
 *
 * @param[in] data the field data
 * @param[in] data_idx the data index
 * @return whether it was successful
 */
static bool set_struct_field_array(s_struct_712_field *field,
                                   const uint8_t *data,
                                   uint8_t *data_idx,
                                   uint8_t length) {
    if ((*data_idx + sizeof(field->array_level_count)) > length)  // check buffer bound
    {
        apdu_response_code = SWO_INCORRECT_DATA;
        return false;
    }
    field->array_level_count = data[(*data_idx)++];
    if ((field->array_levels =
             APP_MEM_ALLOC(sizeof(*field->array_levels) * field->array_level_count)) == NULL) {
        return false;
    }
    for (int idx = 0; idx < field->array_level_count; ++idx) {
        if ((*data_idx + sizeof(field->array_levels[idx].type)) > length)  // check buffer bound
        {
            apdu_response_code = SWO_INCORRECT_DATA;
            return false;
        }
        field->array_levels[idx].type = data[(*data_idx)++];
        switch (field->array_levels[idx].type) {
            case ARRAY_DYNAMIC:  // nothing to do
                break;
            case ARRAY_FIXED_SIZE:
                if ((*data_idx + sizeof(field->array_levels[idx].size)) >
                    length)  // check buffer bound
                {
                    apdu_response_code = SWO_INCORRECT_DATA;
                    return false;
                }
                field->array_levels[idx].size = data[(*data_idx)++];
                break;
            default:
                // should not be in here :^)
                apdu_response_code = SWO_INCORRECT_DATA;
                return false;
        }
    }
    return true;
}

/**
 * Set struct field's type size
 *
 * @param[in] data the field data
 * @param[in,out] data_idx the data index
 * @return whether it was successful
 */
static bool set_struct_field_typesize(s_struct_712_field *field,
                                      const uint8_t *data,
                                      uint8_t *data_idx,
                                      uint8_t length) {
    // copy TypeSize
    if ((*data_idx + sizeof(field->type_size)) > length)  // check buffer bound
    {
        apdu_response_code = SWO_INCORRECT_DATA;
        return false;
    }
    field->type_size = data[(*data_idx)++];
    return true;
}

/**
 * Set struct field's key name
 *
 * @param[in] data the field data
 * @param[in,out] data_idx the data index
 * @return whether it was successful
 */
static bool set_struct_field_keyname(s_struct_712_field *field,
                                     const uint8_t *data,
                                     uint8_t *data_idx,
                                     uint8_t length) {
    uint8_t keyname_len;

    // copy length
    if ((*data_idx + sizeof(keyname_len)) > length)  // check buffer bound
    {
        apdu_response_code = SWO_INCORRECT_DATA;
        return false;
    }
    keyname_len = data[(*data_idx)++];

    // copy name
    if ((*data_idx + keyname_len) > length)  // check buffer bound
    {
        apdu_response_code = SWO_INCORRECT_DATA;
        return false;
    }

    if ((field->key_name = APP_MEM_ALLOC(keyname_len + 1)) == NULL) {
        apdu_response_code = SWO_INSUFFICIENT_MEMORY;
        return false;
    }
    field->key_name[keyname_len] = '\0';
    memmove(field->key_name, &data[*data_idx], keyname_len);
    *data_idx += keyname_len;
    return true;
}

/**
 * Set struct field
 *
 * @param[in] length data length
 * @param[in] data the field data
 * @return whether it was successful
 */
bool set_struct_field(uint8_t length, const uint8_t *data) {
    uint8_t data_idx = 0;

    if ((data == NULL) || (length == 0)) {
        apdu_response_code = SWO_INCORRECT_DATA;
        return false;
    } else if (g_structs == NULL) {
        apdu_response_code = SWO_INCORRECT_DATA;
        return false;
    }

    if (struct_state == NOT_INITIALIZED) {
        apdu_response_code = SWO_INCORRECT_DATA;
        return false;
    }

    s_struct_712_field *new_field = NULL;
    if (APP_MEM_CALLOC((void **) &new_field, sizeof(*new_field)) == false) {
        apdu_response_code = SWO_INSUFFICIENT_MEMORY;
        return false;
    }

    if (!set_struct_field_typedesc(new_field, data, &data_idx, length)) {
        return false;
    }

    // check TypeSize flag in TypeDesc
    if (new_field->type_has_size) {
        // TYPESIZE and TYPE_CUSTOM are mutually exclusive
        if (new_field->type == TYPE_CUSTOM) {
            apdu_response_code = SWO_INCORRECT_DATA;
            return false;
        }

        if (set_struct_field_typesize(new_field, data, &data_idx, length) == false) {
            return false;
        }

    } else if (new_field->type == TYPE_CUSTOM) {
        if (set_struct_field_custom_typename(new_field, data, &data_idx, length) == false) {
            return false;
        }
    }
    if (new_field->type_is_array) {
        if (set_struct_field_array(new_field, data, &data_idx, length) == false) {
            return false;
        }
    }

    if (set_struct_field_keyname(new_field, data, &data_idx, length) == false) {
        return false;
    }

    if (data_idx != length)  // check that there is no more
    {
        apdu_response_code = SWO_INCORRECT_DATA;
        return false;
    }

    // get last struct
    s_struct_712 *s = g_structs;
    while ((s_struct_712 *) ((flist_node_t *) s)->next != NULL) {
        s = (s_struct_712 *) ((flist_node_t *) s)->next;
    }

    flist_push_back((flist_node_t **) &s->fields, (flist_node_t *) new_field);
    return true;
}
