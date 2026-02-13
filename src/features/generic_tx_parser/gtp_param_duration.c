#include "gtp_param_duration.h"
#include "read.h"
#include "gtp_field_table.h"
#include "utils.h"
#include "shared_context.h"
#include "tlv_library.h"
#include "tlv_apdu.h"

#define SECONDS_IN_MINUTE 60
#define MINUTES_IN_HOUR   60
#define SECONDS_IN_HOUR   (SECONDS_IN_MINUTE * MINUTES_IN_HOUR)
#define HOURS_IN_DAY      24
#define MINUTES_IN_DAY    (HOURS_IN_DAY * MINUTES_IN_HOUR)
#define SECONDS_IN_DAY    (MINUTES_IN_DAY * SECONDS_IN_MINUTE)

#define PARAM_DURATION_TAGS(X)                               \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG) \
    X(0x01, TAG_VALUE, handle_value, ENFORCE_UNIQUE_TAG)

static bool handle_version(const tlv_data_t *data, s_param_duration_context *context) {
    return tlv_get_uint8(data, &context->param->version, 0, UINT8_MAX);
}

static bool handle_value(const tlv_data_t *data, s_param_duration_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->value;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return handle_value_struct(&data->value, &ctx);
}

DEFINE_TLV_PARSER(PARAM_DURATION_TAGS, NULL, param_duration_tlv_parser)

bool handle_param_duration_struct(const buffer_t *buf, s_param_duration_context *context) {
    TLV_reception_t received_tags;
    return param_duration_tlv_parser(buf, context, &received_tags);
}

bool format_param_duration(const s_param_duration *param, const char *name) {
    bool ret = true;
    s_parsed_value_collection collec = {0};
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);
    uint16_t days;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint64_t remaining;
    uint8_t raw_buf[sizeof(remaining)] = {0};
    int off;

    if ((ret = value_get(&param->value, &collec))) {
        for (int i = 0; i < collec.size; ++i) {
            off = 0;
            buf_shrink_expand(collec.value[i].ptr,
                              collec.value[i].length,
                              raw_buf,
                              sizeof(raw_buf));
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

            if (!(ret = add_to_field_table(PARAM_TYPE_DURATION, name, buf, NULL))) {
                break;
            }
        }
    }
    value_cleanup(&param->value, &collec);
    return ret;
}
