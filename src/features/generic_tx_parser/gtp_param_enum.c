#include "gtp_param_enum.h"
#include "network.h"
#include "enum_value.h"
#include "gtp_field_table.h"
#include "calldata.h"
#include "shared_context.h"
#include "tx_ctx.h"
#include "tlv_library.h"
#include "tlv_utils.h"

#define PARAM_ENUM_TAGS(X)                                   \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG) \
    X(0x01, TAG_ID, handle_id, ENFORCE_UNIQUE_TAG)           \
    X(0x02, TAG_VALUE, handle_value, ENFORCE_UNIQUE_TAG)

static bool handle_version(const tlv_data_t *data, s_param_enum_context *context) {
    return tlv_get_uint8_range(data, &context->param->version, 0, UINT8_MAX);
}

static bool handle_id(const tlv_data_t *data, s_param_enum_context *context) {
    return tlv_get_uint8_range(data, &context->param->id, 0, UINT8_MAX);
}

static bool handle_value(const tlv_data_t *data, s_param_enum_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->value;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return handle_value_struct(&data->value, &ctx);
}

DEFINE_TLV_PARSER(PARAM_ENUM_TAGS, NULL, param_enum_tlv_parser)

bool handle_param_enum_struct(const buffer_t *buf, s_param_enum_context *context) {
    TLV_reception_t received_tags;
    if (!param_enum_tlv_parser(buf, context, &received_tags)) {
        return false;
    }
    // Enforce the sub-structure's mandatory tags: an empty or partial PARAM
    // payload would otherwise parse fine and never appear in the review
    if (!TLV_CHECK_RECEIVED_TAGS(received_tags, TAG_VERSION, TAG_ID, TAG_VALUE)) {
        PRINTF("Error: missing mandatory tag(s) in gtp_param_enum\n");
        return false;
    }
    return true;
}

bool format_param_enum(const s_param_enum *param, const char *name) {
    bool ret;
    uint64_t chain_id;
    s_parsed_value_collection collec = {0};
    const s_enum_value_entry *enum_entry;
    uint8_t value;
    const uint8_t *selector;
    const uint8_t *callee;

    // Enum entries are 8-bit: the schema must declare an 8-bit unsigned value
    if ((param->value.type_family != TF_UINT) || (param->value.type_size != sizeof(uint8_t))) {
        return false;
    }

    if ((ret = value_get(&param->value, &collec))) {
        if (get_current_tx_info() == NULL) return false;
        chain_id = get_current_tx_info()->chain_id;
        // Enum metadata is signed per contract: use the callee of the current
        // (possibly nested) context, not the root transaction destination.
        if ((callee = get_current_tx_to()) == NULL) {
            return false;
        }
        for (int i = 0; i < collec.size; ++i) {
            if (collec.value[i].length == 0) {
                ret = false;
                break;
            }
            // The calldata word must canonically fit in 8 bits; silently truncating
            // the high bytes would display a label for a different value (e.g. 256
            // shown as the label of 0).
            if (!allzeroes(collec.value[i].ptr, collec.value[i].length - 1)) {
                ret = false;
                break;
            }
            value = collec.value[i].ptr[collec.value[i].length - 1];
            if ((selector = calldata_get_selector(get_current_calldata())) == NULL) {
                ret = false;
                break;
            }
            if ((enum_entry = get_matching_enum(&chain_id, callee, selector, param->id, value)) ==
                NULL) {
                ret = false;
                break;
            }
            if (!(ret = add_to_field_table(PARAM_TYPE_ENUM, name, enum_entry->name, enum_entry))) {
                break;
            }
        }
    }
    value_cleanup(&param->value, &collec);
    return ret;
}
