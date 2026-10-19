#include <stdint.h>
#include <string.h>
#include "gtp_path_array.h"
#include "os_print.h"
#include "utils.h"
#include "tlv_library.h"
#include "tlv_utils.h"

// Define TLV tags for Path Array
#define ARRAY_TAGS(X)                                      \
    X(0x01, TAG_WEIGHT, handle_weight, ENFORCE_UNIQUE_TAG) \
    X(0x02, TAG_START, handle_start, ENFORCE_UNIQUE_TAG)   \
    X(0x03, TAG_END, handle_end, ENFORCE_UNIQUE_TAG)

static bool handle_weight(const tlv_data_t *data, s_path_array_context *context) {
    if (data->value.size < sizeof(context->args->weight)) {
        return false;
    }
    // a zero weight would resolve every element to the first one
    if (data->value.ptr[0] == 0) {
        return false;
    }
    context->args->weight = data->value.ptr[0];
    return true;
}

static bool handle_start(const tlv_data_t *data, s_path_array_context *context) {
    if (!tlv_get_int16(data, &context->args->start)) {
        return false;
    }
    context->args->has_start = true;
    return true;
}

static bool handle_end(const tlv_data_t *data, s_path_array_context *context) {
    if (!tlv_get_int16(data, &context->args->end)) {
        return false;
    }
    context->args->has_end = true;
    return true;
}

// Generate TLV parser for Path Array
DEFINE_TLV_PARSER(ARRAY_TAGS, NULL, array_tlv_parser)

bool handle_array_struct(const buffer_t *buf, s_path_array_context *context) {
    TLV_reception_t received_tags;
    return array_tlv_parser(buf, context, &received_tags);
}
