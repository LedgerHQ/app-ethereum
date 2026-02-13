#include <stdint.h>
#include <string.h>
#include "gtp_path_array.h"
#include "read.h"
#include "os_print.h"
#include "utils.h"
#include "tlv_library.h"

// Define TLV tags for Path Array
#define ARRAY_TAGS(X)                                      \
    X(0x01, TAG_WEIGHT, handle_weight, ENFORCE_UNIQUE_TAG) \
    X(0x02, TAG_START, handle_start, ENFORCE_UNIQUE_TAG)   \
    X(0x03, TAG_END, handle_end, ENFORCE_UNIQUE_TAG)

static bool handle_weight(const tlv_data_t *data, s_path_array_context *context) {
    context->args->weight = data->value.ptr[0];
    return true;
}

static bool handle_start(const tlv_data_t *data, s_path_array_context *context) {
    context->args->start = read_u16_be(data->value.ptr, 0);
    context->args->has_start = true;
    return true;
}

static bool handle_end(const tlv_data_t *data, s_path_array_context *context) {
    context->args->end = read_u16_be(data->value.ptr, 0);
    context->args->has_end = true;
    return true;
}

// Generate TLV parser for Path Array
DEFINE_TLV_PARSER(ARRAY_TAGS, NULL, array_tlv_parser)

bool handle_array_struct(const buffer_t *buf, s_path_array_context *context) {
    TLV_reception_t received_tags;
    return array_tlv_parser(buf, context, &received_tags);
}
