#include "gtp_path_slice.h"
#include "os_print.h"
#include "read.h"
#include "tlv_library.h"

// Define TLV tags for Path Slice
#define SLICE_TAGS(X)                                    \
    X(0x01, TAG_START, handle_start, ENFORCE_UNIQUE_TAG) \
    X(0x02, TAG_END, handle_end, ENFORCE_UNIQUE_TAG)

static bool handle_start(const tlv_data_t *data, s_path_slice_context *context) {
    if (data->value.size < 2) {
        return false;
    }
    uint16_t value = read_u16_be(data->value.ptr, 0);
    context->args->start = value;
    context->args->has_start = true;
    return true;
}

static bool handle_end(const tlv_data_t *data, s_path_slice_context *context) {
    if (data->value.size < 2) {
        return false;
    }
    uint16_t value = read_u16_be(data->value.ptr, 0);
    context->args->end = value;
    context->args->has_end = true;
    return true;
}

// Generate TLV parser for Path Slice
DEFINE_TLV_PARSER(SLICE_TAGS, NULL, slice_tlv_parser)

/**
 * Wrapper to parse Path Slice TLV payload.
 *
 * @param[in] buf TLV buffer
 * @param[out] context Path Slice context
 * @return whether parsing was successful
 */
bool handle_slice_struct(const buffer_t *buf, s_path_slice_context *context) {
    TLV_reception_t received_tags;
    return slice_tlv_parser(buf, context, &received_tags);
}
