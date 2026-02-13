#include "gtp_param_datetime.h"
#include "gtp_field_table.h"
#include "read.h"
#include "time_format.h"
#include "utils.h"
#include "shared_context.h"
#include "tlv_library.h"
#include "tlv_apdu.h"

#define PARAM_DATETIME_TAGS(X)                               \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG) \
    X(0x01, TAG_VALUE, handle_value, ENFORCE_UNIQUE_TAG)     \
    X(0x02, TAG_TYPE, handle_type, ENFORCE_UNIQUE_TAG)

static bool handle_version(const tlv_data_t *data, s_param_datetime_context *context) {
    return tlv_get_uint8(data, &context->param->version, 0, UINT8_MAX);
}

static bool handle_value(const tlv_data_t *data, s_param_datetime_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->value;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return handle_value_struct(&data->value, &ctx);
}

static bool handle_type(const tlv_data_t *data, s_param_datetime_context *context) {
    if (!tlv_get_uint8(data, &context->param->type, 0, UINT8_MAX)) {
        return false;
    }
    switch (context->param->type) {
        case DT_UNIX:
        case DT_BLOCKHEIGHT:
            break;
        default:
            return false;
    }
    return true;
}

DEFINE_TLV_PARSER(PARAM_DATETIME_TAGS, NULL, param_datetime_tlv_parser)

bool handle_param_datetime_struct(const buffer_t *buf, s_param_datetime_context *context) {
    TLV_reception_t received_tags;
    return param_datetime_tlv_parser(buf, context, &received_tags);
}

bool format_param_datetime(const s_param_datetime *param, const char *name) {
    bool ret;
    s_parsed_value_collection collec = {0};
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);
    uint8_t time_buf[sizeof(time_t)] = {0};
    time_t timestamp;
    uint256_t block_height;

    if ((ret = value_get(&param->value, &collec))) {
        for (int i = 0; i < collec.size; ++i) {
            if (param->type == DT_UNIX) {
                if ((collec.value[i].length >= param->value.type_size) &&
                    ismaxint((uint8_t *) collec.value[i].ptr, collec.value[i].length)) {
                    snprintf(buf, buf_size, "Unlimited");
                } else {
                    buf_shrink_expand(collec.value[i].ptr,
                                      collec.value[i].length,
                                      time_buf,
                                      sizeof(time_buf));
                    timestamp = read_u64_be(time_buf, 0);
                    if (!(ret = time_format_to_utc(&timestamp, buf, buf_size))) {
                        break;
                    }
                }
            } else if (param->type == DT_BLOCKHEIGHT) {
                convertUint256BE(collec.value[i].ptr, collec.value[i].length, &block_height);
                if (!(ret = tostring256(&block_height, 10, buf, buf_size))) {
                    break;
                }
            }
            if (!(ret = add_to_field_table(PARAM_TYPE_DATETIME, name, buf, NULL))) {
                break;
            }
        }
    }
    value_cleanup(&param->value, &collec);
    return ret;
}
