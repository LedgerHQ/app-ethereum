#ifdef HAVE_GENERIC_TX_PARSER

#include <stdint.h>
#include <string.h>
#include "gtp_path_array.h"
#include "read.h"
#include "os_print.h"
#include "utils.h"

enum {
    TAG_WEIGHT = 0x01,
    TAG_START = 0x02,
    TAG_END = 0x03,
};

static bool handle_weight(const s_tlv_data *data, s_path_array_context *context) {
    if (data->length != sizeof(context->args->weight)) {
        return false;
    }
    context->args->weight = data->value[0];
    return true;
}

static bool handle_start(const s_tlv_data *data, s_path_array_context *context) {
    if (data->length != sizeof(context->args->start)) {
        return false;
    }
    context->args->start = read_u16_be(data->value, 0);
    context->args->has_start = true;
    return true;
}

static bool handle_end(const s_tlv_data *data, s_path_array_context *context) {
    if (data->length != sizeof(context->args->end)) {
        return false;
    }
    context->args->end = read_u16_be(data->value, 0);
    context->args->has_end = true;
    return true;
}

bool handle_array_struct(const s_tlv_data *data, s_path_array_context *context) {
    bool ret;

    switch (data->tag) {
        case TAG_WEIGHT:
            ret = handle_weight(data, context);
            break;
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
