#include "gtp_param_unit.h"
#include "gtp_field_table.h"
#include "shared_context.h"
#include "tlv_library.h"
#include "tlv_apdu.h"

#define PARAM_UNIT_TAGS(X)                                     \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG)   \
    X(0x01, TAG_VALUE, handle_value, ENFORCE_UNIQUE_TAG)       \
    X(0x02, TAG_BASE, handle_base, ENFORCE_UNIQUE_TAG)         \
    X(0x03, TAG_DECIMALS, handle_decimals, ENFORCE_UNIQUE_TAG) \
    X(0x04, TAG_PREFIX, handle_prefix, ENFORCE_UNIQUE_TAG)

static bool handle_version(const tlv_data_t *data, s_param_unit_context *context) {
    return tlv_get_uint8(data, &context->param->version, 0, UINT8_MAX);
}

static bool handle_value(const tlv_data_t *data, s_param_unit_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->value;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return handle_value_struct(&data->value, &ctx);
}

static bool handle_base(const tlv_data_t *data, s_param_unit_context *context) {
    if (data->value.size >= sizeof(context->param->base)) {
        return false;
    }
    memcpy(context->param->base, data->value.ptr, data->value.size);
    context->param->base[data->value.size] = '\0';
    return true;
}

static bool handle_decimals(const tlv_data_t *data, s_param_unit_context *context) {
    return tlv_get_uint8(data, &context->param->decimals, 0, UINT8_MAX);
}

static bool handle_prefix(const tlv_data_t *data, s_param_unit_context *context) {
    if (data->value.size != sizeof(bool)) {
        return false;
    }
    (void) context;
    // unused for now
    return true;
}

DEFINE_TLV_PARSER(PARAM_UNIT_TAGS, NULL, param_unit_tlv_parser)

bool handle_param_unit_struct(const buffer_t *buf, s_param_unit_context *context) {
    TLV_reception_t received_tags;
    return param_unit_tlv_parser(buf, context, &received_tags);
}

bool format_param_unit(const s_param_unit *param, const char *name) {
    bool ret;
    s_parsed_value_collection collec = {0};
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);
    char tmp[80];
    size_t off;

    if ((ret = value_get(&param->value, &collec))) {
        for (int i = 0; i < collec.size; ++i) {
            if (!(ret = uint256_to_decimal(collec.value[i].ptr,
                                           collec.value[i].length,
                                           tmp,
                                           sizeof(tmp)))) {
                break;
            }
            if (!(ret = adjustDecimals(tmp,
                                       strnlen(tmp, sizeof(tmp)),
                                       buf,
                                       buf_size,
                                       param->decimals))) {
                break;
            }
            if (param->base[0] == '\0') {
                ret = false;
                break;
            }
            off = strlen(buf);
            snprintf(&buf[off], buf_size - off, " %s", param->base);

            if (!(ret = add_to_field_table(PARAM_TYPE_UNIT, name, buf, NULL))) {
                break;
            }
        }
    }
    value_cleanup(&param->value, &collec);
    return ret;
}
