#ifdef HAVE_GENERIC_TX_PARSER

#include "gtp_param_duration.h"
#include "read.h"
#include "gtp_field_table.h"
#include "utils.h"
#include "shared_context.h"

#define SECONDS_IN_MINUTE 60
#define MINUTES_IN_HOUR   60
#define SECONDS_IN_HOUR   (SECONDS_IN_MINUTE * MINUTES_IN_HOUR)
#define HOURS_IN_DAY      24
#define MINUTES_IN_DAY    (HOURS_IN_DAY * MINUTES_IN_HOUR)
#define SECONDS_IN_DAY    (MINUTES_IN_DAY * SECONDS_IN_MINUTE)

enum {
    TAG_VERSION = 0x00,
    TAG_VALUE = 0x01,
};

static bool handle_version(const s_tlv_data *data, s_param_duration_context *context) {
    if (data->length != sizeof(context->param->version)) {
        return false;
    }
    context->param->version = data->value[0];
    return true;
}

static bool handle_value(const s_tlv_data *data, s_param_duration_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->value;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_value_struct, &ctx);
}

bool handle_param_duration_struct(const s_tlv_data *data, s_param_duration_context *context) {
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

bool format_param_duration(const s_param_duration *param, const char *name) {
    s_parsed_value_collection collec;
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);
    uint16_t days;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint64_t remaining;
    uint8_t raw_buf[sizeof(remaining)] = {0};
    int off;

    if (!value_get(&param->value, &collec)) {
        return false;
    }
    for (int i = 0; i < collec.size; ++i) {
        off = 0;
        buf_shrink_expand(collec.value[i].ptr, collec.value[i].length, raw_buf, sizeof(raw_buf));
        remaining = read_u64_be(raw_buf, 0);

        days = remaining / SECONDS_IN_DAY;
        if (days > 0) {
            snprintf(&buf[off], buf_size - off, "%dd", days);
            off = strlen(buf);
        }
        remaining %= SECONDS_IN_DAY;

        hours = remaining / SECONDS_IN_HOUR;
        if (hours > 0) {
            snprintf(&buf[off], buf_size - off, "%02dh", hours);
            off = strlen(buf);
        }
        remaining %= SECONDS_IN_HOUR;

        minutes = remaining / SECONDS_IN_MINUTE;
        if (minutes > 0) {
            snprintf(&buf[off], buf_size - off, "%02dm", minutes);
            off = strlen(buf);
        }
        remaining %= SECONDS_IN_MINUTE;

        seconds = (uint8_t) remaining;
        if ((seconds > 0) || (off == 0)) {
            snprintf(&buf[off], buf_size - off, "%02ds", seconds);
        }

        if (!add_to_field_table(PARAM_TYPE_DURATION, name, buf)) {
            return false;
        }
    }
    return true;
}

#endif  // HAVE_GENERIC_TX_PARSER
