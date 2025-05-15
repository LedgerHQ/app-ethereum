#include "gtp_param_datetime.h"
#include "gtp_field_table.h"
#include "read.h"
#include "time_format.h"
#include "utils.h"
#include "shared_context.h"

enum {
    TAG_VERSION = 0x00,
    TAG_VALUE = 0x01,
    TAG_TYPE = 0x02,
};

static bool handle_version(const s_tlv_data *data, s_param_datetime_context *context) {
    if (data->length != sizeof(context->param->version)) {
        return false;
    }
    context->param->version = data->value[0];
    return true;
}

static bool handle_value(const s_tlv_data *data, s_param_datetime_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->value;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_value_struct, &ctx);
}

static bool handle_type(const s_tlv_data *data, s_param_datetime_context *context) {
    if (data->length != sizeof(context->param->type)) {
        return false;
    }
    switch (data->value[0]) {
        case DT_UNIX:
        case DT_BLOCKHEIGHT:
            break;
        default:
            return false;
    }
    context->param->type = data->value[0];
    return true;
}

bool handle_param_datetime_struct(const s_tlv_data *data, s_param_datetime_context *context) {
    bool ret;

    switch (data->tag) {
        case TAG_VERSION:
            ret = handle_version(data, context);
            break;
        case TAG_VALUE:
            ret = handle_value(data, context);
            break;
        case TAG_TYPE:
            ret = handle_type(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    return ret;
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
            if (!(ret = add_to_field_table(PARAM_TYPE_DATETIME, name, buf))) {
                break;
            }
        }
    }
    value_cleanup(&param->value, &collec);
    return ret;
}
