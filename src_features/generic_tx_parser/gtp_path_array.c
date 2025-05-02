#ifdef HAVE_GENERIC_TX_PARSER

#include <stdint.h>
#include <string.h>
#include "gtp_path_array.h"
#include "read.h"
#include "tlv_library.h"
#include "buffer.h"
#include "os_print.h"
#include "utils.h"

static bool handle_weight(const tlv_data_t *data, s_path_array_context *context) {
    return get_uint8_t_from_tlv_data(data, &context->args->weight);
}

static bool handle_start(const tlv_data_t *data, s_path_array_context *context) {
    if (data->length != sizeof(context->args->start)) {
        return false;
    }
    context->args->start = read_u16_be(data->value, 0);
    context->args->has_start = true;
    return true;
}

static bool handle_end(const tlv_data_t *data, s_path_array_context *context) {
    if (data->length != sizeof(context->args->end)) {
        return false;
    }
    context->args->end = read_u16_be(data->value, 0);
    context->args->has_end = true;
    return true;
}

// clang-format off
#define TLV_TAGS(X)                                        \
    X(0x01, TAG_WEIGHT, handle_weight, ALLOW_MULTIPLE_TAG) \
    X(0x02, TAG_START,  handle_start,  ENFORCE_UNIQUE_TAG) \
    X(0x03, TAG_END,    handle_end,    ENFORCE_UNIQUE_TAG)

DEFINE_TLV_PARSER(TLV_TAGS, parse_tlv_array)

bool handle_array_struct(const s_tlv_data *data, s_path_array_context *context) {
    buffer_t payload_buffer = {.ptr = data->value, .size = data->length};
    return parse_tlv_array(&payload_buffer, context, NULL);
}

#endif  // HAVE_GENERIC_TX_PARSER
