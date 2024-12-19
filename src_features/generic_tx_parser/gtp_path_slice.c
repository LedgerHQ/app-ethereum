#ifdef HAVE_GENERIC_TX_PARSER

#include "gtp_path_slice.h"
#include "os_print.h"
#include "read.h"

enum {
    TAG_START = 0x01,
    TAG_END = 0x02,
};

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

bool handle_slice_struct(const s_tlv_data *data, s_path_slice_context *context) {
    bool ret;

    switch (data->tag) {
        case TAG_START:
            ret = handle_start(data, context);
            break;
        case TAG_END:
            ret = handle_end(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    return ret;
}

#endif  // HAVE_GENERIC_TX_PARSER
