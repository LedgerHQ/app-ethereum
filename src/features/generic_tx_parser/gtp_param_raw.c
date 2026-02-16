#include <inttypes.h>
#include "os_print.h"
#include "gtp_param_raw.h"
#include "gtp_field.h"
#include "uint256.h"
#include "read.h"
#include "gtp_field_table.h"
#include "utils.h"
#include "shared_context.h"

enum {
    TAG_VERSION = 0x00,
    TAG_VALUE = 0x01,
};

static bool handle_version(const s_tlv_data *data, s_param_raw_context *context) {
    if (data->length != sizeof(context->param->version)) {
        return false;
    }
    context->param->version = data->value[0];
    return true;
}

static bool handle_value(const s_tlv_data *data, s_param_raw_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->value;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_value_struct, &ctx);
}

bool handle_param_raw_struct(const s_tlv_data *data, s_param_raw_context *context) {
    bool ret;

    switch (data->tag) {
        case TAG_VERSION:
            ret = handle_version(data, context);
            break;
        case TAG_VALUE:
            ret = handle_value(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    return ret;
}

/**
 * @brief Apply visibility constraint logic
 *
 * @param field Field with visibility setting
 * @param to_be_displayed Output: whether field should be displayed
 * @param constraint_matched Whether value matched a constraint
 * @param value_type Type name for error messages
 * @return false if TX should be rejected (MUST_BE not matched), true otherwise
 */
static bool apply_visibility_constraint(const s_field *field,
                                        bool *to_be_displayed,
                                        bool constraint_matched) {
    *to_be_displayed = false;

    switch (field->visibility) {
        case PARAM_VISIBILITY_MUST_BE:
            if (!constraint_matched) {
                PRINTF("Error: RAW value does not match any MUST_BE constraint!\n");
                // Reject the TX
                return false;
            }
            break;
        case PARAM_VISIBILITY_IF_NOT_IN:
            if (constraint_matched) {
                PRINTF("Warning: RAW value does match a IF_NOT_IN constraint!\n");
                // Skip displaying the field
                break;
            }
            *to_be_displayed = true;
            break;
        default:  // PARAM_VISIBILITY_ALWAYS
            *to_be_displayed = true;
            break;
    }

    return true;
}

/**
 * @brief Check if a uint256 value matches any of the field's constraints
 *
 * @param field Field containing the constraints to check
 * @param value256 Value to check against constraints
 * @return true if value matches a constraint, false otherwise
 */
static bool check_uint_constraint(const s_field *field, const uint256_t *value256) {
    uint256_t constraint = {0};

    for (s_field_constraint *c_node = field->constraints; c_node != NULL;
         c_node = (s_field_constraint *) c_node->node.next) {
        memset(&constraint, 0, sizeof(constraint));
        convertUint256BE(c_node->value, c_node->size, &constraint);
        if (equal256(value256, &constraint)) {
            return true;
        }
    }
    return false;
}

bool format_uint(const s_field *field,
                 bool *to_be_displayed,
                 s_parsed_value *value,
                 char *buf,
                 size_t buf_size) {
    uint256_t value256 = {0};

    convertUint256BE(value->ptr, value->length, &value256);

    if (!apply_visibility_constraint(field,
                                     to_be_displayed,
                                     check_uint_constraint(field, &value256))) {
        return false;
    }

    return *to_be_displayed ? tostring256(&value256, 10, buf, buf_size) : true;
}

bool format_int(const s_value *def, const s_parsed_value *value, char *buf, size_t buf_size) {
    uint8_t tmp[32];
    bool ret;
    union {
        uint256_t value256;
        uint128_t value128;
        int64_t value64;
        int32_t value32;
        int16_t value16;
        int8_t value8;
    } uv;

    buf_shrink_expand(value->ptr, value->length, tmp, def->type_size);
    switch (def->type_size * 8) {
        case 256:
            convertUint256BE(tmp, def->type_size, &uv.value256);
            ret = tostring256_signed(&uv.value256, 10, buf, buf_size);
            break;
        case 128:
            convertUint128BE(tmp, def->type_size, &uv.value128);
            ret = tostring128_signed(&uv.value128, 10, buf, buf_size);
            break;
        case 64:
            uv.value64 = read_u64_be(tmp, 0);
            ret = snprintf(buf, buf_size, "%" PRId64, uv.value64) > 0;
            break;
        case 32:
            uv.value32 = read_u32_be(tmp, 0);
            ret = snprintf(buf, buf_size, "%" PRId32, uv.value32) > 0;
            break;
        case 16:
            uv.value16 = read_u16_be(tmp, 0);
            ret = snprintf(buf, buf_size, "%u" PRId16, uv.value16) > 0;
            break;
        case 8:
            uv.value8 = value->ptr[0];
            ret = snprintf(buf, buf_size, "%u" PRId8, uv.value8) > 0;
            break;
        default:
            ret = false;
    }
    return ret;
}

/**
 * @brief Check if an address matches any of the field's constraints
 *
 * @param field Field containing the constraints to check
 * @param addr Address to check against constraints
 * @return true if address matches a constraint, false otherwise
 */
static bool check_address_constraint(const s_field *field, const uint8_t *addr) {
    uint8_t constraint[ADDRESS_LENGTH] = {0};

    for (s_field_constraint *c_node = field->constraints; c_node != NULL;
         c_node = (s_field_constraint *) c_node->node.next) {
        memset(constraint, 0, sizeof(constraint));
        buf_shrink_expand(c_node->value, c_node->size, constraint, sizeof(constraint));
        if (memcmp(addr, constraint, ADDRESS_LENGTH) == 0) {
            return true;
        }
    }
    return false;
}

static bool format_addr(const s_field *field,
                        bool *to_be_displayed,
                        const s_parsed_value *value,
                        char *buf,
                        size_t buf_size) {
    uint8_t tmp[ADDRESS_LENGTH] = {0};

    buf_shrink_expand(value->ptr, value->length, tmp, sizeof(tmp));

    if (!apply_visibility_constraint(field,
                                     to_be_displayed,
                                     check_address_constraint(field, tmp))) {
        return false;
    }

    return *to_be_displayed ? getEthDisplayableAddress(tmp, buf, buf_size, chainConfig->chainId)
                            : true;
}

static bool format_bool(const s_value *def,
                        const s_parsed_value *value,
                        char *buf,
                        size_t buf_size) {
    uint8_t tmp;

    (void) def;
    buf_shrink_expand(value->ptr, value->length, &tmp, 1);
    snprintf(buf, buf_size, "%s", tmp ? "true" : "false");
    return true;
}

/**
 * @brief Check if a bytes value matches any of the field's constraints
 *
 * @param field Field containing the constraints to check
 * @param value Value being formatted
 * @param formatted_buf Formatted buffer containing the hex string to check
 * @return true if value matches a constraint, false otherwise
 */
static bool check_bytes_constraint(const s_field *field,
                                   const s_parsed_value *value,
                                   const char *formatted_buf) {
    char constraint[sizeof(strings.tmp.tmp)] = {0};

    for (s_field_constraint *c_node = field->constraints; c_node != NULL;
         c_node = (s_field_constraint *) c_node->node.next) {
        if (c_node->size > value->length) {
            PRINTF("Warning: RAW BYTES constraint wrong size!\n");
            continue;
        }
        memset(constraint, 0, sizeof(constraint));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
        snprintf(constraint, sizeof(constraint), "0x%.*h", c_node->size, c_node->value);
#pragma GCC diagnostic pop
        if (strcmp(formatted_buf, constraint) == 0) {
            return true;
        }
    }
    return false;
}

static bool format_bytes(const s_field *field,
                         bool *to_be_displayed,
                         const s_parsed_value *value,
                         char *buf,
                         size_t buf_size) {
    LEDGER_ASSERT(sizeof(strings.tmp.tmp) == buf_size, "Buffer too small for bytes formatting");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
    snprintf(buf, buf_size, "0x%.*h", value->length, value->ptr);
#pragma GCC diagnostic pop

    if (!apply_visibility_constraint(field,
                                     to_be_displayed,
                                     check_bytes_constraint(field, value, buf))) {
        return false;
    }

    if (!*to_be_displayed) {
        return true;
    }

    // Truncate if needed for display
    if ((2 + (value->length * 2) + 1) > (int) buf_size) {
        memmove(&buf[buf_size - 1 - 3], "...", 3);
    }
    return true;
}

static bool format_string(const s_value *def,
                          const s_parsed_value *value,
                          char *buf,
                          size_t buf_size) {
    (void) def;
    str_cpy_explicit_trunc((char *) value->ptr, value->length, buf, buf_size);
    return true;
}

bool format_param_raw(const s_field *field) {
    bool ret = false;
    s_parsed_value_collection collec = {0};
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);
    bool to_be_displayed = true;

    if (field->visibility >= PARAM_VISIBILITY_MAX) {
        PRINTF("Error: Unsupported visibility (%u)\n", field->visibility);
        return false;
    }

    ret = value_get(&field->param_raw.value, &collec);
    if (ret) {
        for (int i = 0; i < collec.size && ret == true; ++i) {
            switch (field->param_raw.value.type_family) {
                case TF_UINT:
                    ret = format_uint(field, &to_be_displayed, &collec.value[i], buf, buf_size);
                    break;
                case TF_INT:
                    ret = format_int(&field->param_raw.value, &collec.value[i], buf, buf_size);
                    break;
                case TF_ADDRESS:
                    ret = format_addr(field, &to_be_displayed, &collec.value[i], buf, buf_size);
                    break;
                case TF_BOOL:
                    ret = format_bool(&field->param_raw.value, &collec.value[i], buf, buf_size);
                    break;
                case TF_BYTES:
                    ret = format_bytes(field, &to_be_displayed, &collec.value[i], buf, buf_size);
                    break;
                case TF_STRING:
                    ret = format_string(&field->param_raw.value, &collec.value[i], buf, buf_size);
                    break;
                case TF_UFIXED:
                case TF_FIXED:
                default:
                    ret = false;
            }
            // Add to field table only if required to be displayed
            if (ret && to_be_displayed) {
                ret = add_to_field_table(PARAM_TYPE_RAW, field->name, buf, NULL);
            }
        }
    }
    value_cleanup(&field->param_raw.value, &collec);
    return ret;
}
