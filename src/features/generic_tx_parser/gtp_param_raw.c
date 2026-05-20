#include "os.h"
#include "os_print.h"
#include "common_utils.h"
#include "gtp_param_raw.h"
#include "gtp_field.h"
#include "uint256.h"
#include "gtp_field_table.h"
#include "utils.h"
#include "shared_context.h"
#include "tlv_library.h"
#include "tlv_utils.h"

#define PARAM_RAW_TAGS(X)                                    \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG) \
    X(0x01, TAG_VALUE, handle_value, ENFORCE_UNIQUE_TAG)

static bool handle_version(const tlv_data_t *data, s_param_raw_context *context) {
    return tlv_get_uint8_range(data, &context->param->version, 0, UINT8_MAX);
}

static bool handle_value(const tlv_data_t *data, s_param_raw_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->value;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return handle_value_struct(&data->value, &ctx);
}

DEFINE_TLV_PARSER(PARAM_RAW_TAGS, NULL, param_raw_tlv_parser)

bool handle_param_raw_struct(const buffer_t *buf, s_param_raw_context *context) {
    TLV_reception_t received_tags;
    return param_raw_tlv_parser(buf, context, &received_tags);
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
    const uint8_t zero = 0;
    const uint8_t *value_ptr = value->ptr;

    if (value->length == 0) {
        value_ptr = &zero;
    } else if (value_ptr == NULL) {
        return false;
    }

    convertUint256BE(value_ptr, value->length, &value256);

    if (!apply_visibility_constraint(field,
                                     to_be_displayed,
                                     check_uint_constraint(field, &value256))) {
        return false;
    }

    return *to_be_displayed ? uint256_to_decimal(value_ptr, value->length, buf, buf_size) : true;
}

/**
 * @brief Check if a signed integer value matches any of the field's constraints
 *
 * Constraints are compared as canonical decimal strings so the same logic
 * works regardless of the byte length and sign-extension of either side.
 */
static bool check_int_constraint(const s_field *field, const char *formatted_buf) {
    char constraint_buf[sizeof(strings.tmp.tmp)];

    for (s_field_constraint *c_node = field->constraints; c_node != NULL;
         c_node = (s_field_constraint *) c_node->node.next) {
        if (!format_signed_int_be(c_node->value,
                                  c_node->size,
                                  field->param_raw.value.type_size,
                                  constraint_buf,
                                  sizeof(constraint_buf))) {
            continue;
        }
        if (strcmp(formatted_buf, constraint_buf) == 0) {
            return true;
        }
    }
    return false;
}

bool format_int(const s_field *field,
                bool *to_be_displayed,
                const s_parsed_value *value,
                char *buf,
                size_t buf_size) {
    if (!format_signed_int_be(value->ptr,
                              value->length,
                              field->param_raw.value.type_size,
                              buf,
                              buf_size)) {
        return false;
    }
    return apply_visibility_constraint(field, to_be_displayed, check_int_constraint(field, buf));
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

    return *to_be_displayed ? getEthDisplayableAddress(tmp, buf, buf_size, g_chain_config->chain_id)
                            : true;
}

/**
 * @brief Check if a bool value matches any of the field's constraints
 */
static bool check_bool_constraint(const s_field *field, uint8_t value) {
    uint8_t cv;

    for (s_field_constraint *c_node = field->constraints; c_node != NULL;
         c_node = (s_field_constraint *) c_node->node.next) {
        // Normalize to 0/1 — a constraint may be encoded as any non-zero byte
        // and may be sign-extended across multiple bytes.
        cv = 0;
        for (uint32_t i = 0; i < c_node->size; ++i) {
            if (c_node->value[i] != 0) {
                cv = 1;
                break;
            }
        }
        if (cv == (value ? 1 : 0)) {
            return true;
        }
    }
    return false;
}

static bool format_bool(const s_field *field,
                        bool *to_be_displayed,
                        const s_parsed_value *value,
                        char *buf,
                        size_t buf_size) {
    uint8_t tmp;

    buf_shrink_expand(value->ptr, value->length, &tmp, 1);
    snprintf(buf, buf_size, "%s", tmp ? "true" : "false");
    return apply_visibility_constraint(field, to_be_displayed, check_bool_constraint(field, tmp));
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
        if (sizeof(constraint) < 3) {
            continue;
        }
        constraint[0] = '0';
        constraint[1] = 'x';
        if (bytes_to_lowercase_hex(constraint + 2,
                                   sizeof(constraint) - 2,
                                   c_node->value,
                                   c_node->size) != 0) {
            continue;
        }
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

    // "0x" prefix + two hex digits per byte + NULL terminator. Reject upfront
    // so the rejection is self-documenting rather than implied by
    // bytes_to_lowercase_hex's internal size check, and the caller gets a
    // clean ERROR APDU instead of a silently truncated review screen.
    const size_t needed = (size_t) 2 + (size_t) value->length * 2 + 1;
    if (needed > buf_size) {
        PRINTF("RAW BYTES value too long for display (%u > %u bytes)\n",
               (unsigned) needed,
               (unsigned) buf_size);
        return false;
    }
    buf[0] = '0';
    buf[1] = 'x';
    if (bytes_to_lowercase_hex(buf + 2, buf_size - 2, value->ptr, value->length) != 0) {
        return false;
    }

    if (!apply_visibility_constraint(field,
                                     to_be_displayed,
                                     check_bytes_constraint(field, value, buf))) {
        return false;
    }

    return true;
}

/**
 * @brief Check if a string value matches any of the field's constraints
 *
 * Byte-level equality: constraint and parsed value must have the same length
 * and identical contents. The constraint is treated as the raw bytes from
 * the TLV, not as a NUL-terminated string.
 */
static bool check_string_constraint(const s_field *field, const s_parsed_value *value) {
    for (s_field_constraint *c_node = field->constraints; c_node != NULL;
         c_node = (s_field_constraint *) c_node->node.next) {
        if ((c_node->size == value->length) &&
            (memcmp(c_node->value, value->ptr, c_node->size) == 0)) {
            return true;
        }
    }
    return false;
}

static bool format_string(const s_field *field,
                          bool *to_be_displayed,
                          const s_parsed_value *value,
                          char *buf,
                          size_t buf_size) {
    str_cpy_explicit_trunc((char *) value->ptr, value->length, buf, buf_size);
    return apply_visibility_constraint(field,
                                       to_be_displayed,
                                       check_string_constraint(field, value));
}

static bool format_separator(const char *tmpl, int idx) {
    char buf[MAX_SEPARATOR_SIZE];
    const char *placeholder = "{index}";
    const char *pos = strstr(tmpl, placeholder);

    if (pos == NULL) {
        return add_to_field_table(PARAM_TYPE_SEPARATOR, tmpl, "", NULL);
    }

    char idx_str[12];
    snprintf(idx_str, sizeof(idx_str), "%d", idx);
    snprintf(buf,
             sizeof(buf),
             "%.*s%s%s",
             (int) (pos - tmpl),
             tmpl,
             idx_str,
             pos + strlen(placeholder));
    return add_to_field_table(PARAM_TYPE_SEPARATOR, buf, "", NULL);
}

bool format_param_raw(const s_field *field) {
    bool ret = false;
    s_parsed_value_collection collec = {0};
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);
    bool to_be_displayed = true;

    ret = value_get(&field->param_raw.value, &collec);
    if (ret) {
        for (int i = 0; i < collec.size && ret == true; ++i) {
            // Reset on each iteration: PARAM_VISIBILITY_IF_NOT_IN can hide a
            // single value without disabling display for the rest of the
            // collection (CWE-451 / CWE-693).
            to_be_displayed = true;
            switch (field->param_raw.value.type_family) {
                case TF_UINT:
                    ret = format_uint(field, &to_be_displayed, &collec.value[i], buf, buf_size);
                    break;
                case TF_INT:
                    ret = format_int(field, &to_be_displayed, &collec.value[i], buf, buf_size);
                    break;
                case TF_ADDRESS:
                    ret = format_addr(field, &to_be_displayed, &collec.value[i], buf, buf_size);
                    break;
                case TF_BOOL:
                    ret = format_bool(field, &to_be_displayed, &collec.value[i], buf, buf_size);
                    break;
                case TF_BYTES:
                    ret = format_bytes(field, &to_be_displayed, &collec.value[i], buf, buf_size);
                    break;
                case TF_STRING:
                    ret = format_string(field, &to_be_displayed, &collec.value[i], buf, buf_size);
                    break;
                case TF_UFIXED:
                case TF_FIXED:
                default:
                    ret = false;
            }
            // Emit separator (page break or labeled header) before each element
            if (ret && to_be_displayed && field->separator[0] != '\0') {
                ret = format_separator(field->separator, i + 1);
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
