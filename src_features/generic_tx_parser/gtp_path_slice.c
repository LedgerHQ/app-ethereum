#ifdef HAVE_GENERIC_TX_PARSER

#include "gtp_path_slice.h"
#include "os_print.h"
#include "tlv_library.h"
#include "read.h"

static bool handle_start(const s_tlv_data *data, s_path_slice_context *context) {
    if (data->length != sizeof(context->args->start)) {
        return false;
    }
    context->args->start = read_u16_be(data->value, 0);
    context->args->has_start = true;
    return true;
}

static bool handle_end(const s_tlv_data *data, s_path_slice_context *context) {
    if (data->length != sizeof(context->args->end)) {
        return false;
    }
    context->args->end = read_u16_be(data->value, 0);
    context->args->has_end = true;
    return true;
}

// clang-format off
#define TLV_TAGS(X)                                      \
    X(0x01, TAG_START, handle_start, ENFORCE_UNIQUE_TAG) \
    X(0x02, TAG_END,   handle_end,   ENFORCE_UNIQUE_TAG)

DEFINE_TLV_PARSER(TLV_TAGS, parse_tlv_slice)

bool handle_slice_struct(const s_tlv_data *data, s_path_slice_context *context) {
    buffer_t payload_buffer = {.ptr = data->value, .size = data->length};
    return parse_tlv_slice(&payload_buffer, context, NULL);
}

#endif  // HAVE_GENERIC_TX_PARSER
