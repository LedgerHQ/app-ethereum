#include "gtp_path_slice.h"
#include "os_print.h"
#include "tlv_library.h"
#include "tlv_utils.h"

// Define TLV tags for Path Slice
#define SLICE_TAGS(X)                                    \
    X(0x01, TAG_START, handle_start, ENFORCE_UNIQUE_TAG) \
    X(0x02, TAG_END, handle_end, ENFORCE_UNIQUE_TAG)

static bool handle_start(const tlv_data_t *data, s_path_slice_context *context) {
    if (!tlv_get_int16(data, &context->args->start)) {
        return false;
    }
    context->args->has_start = true;
    return true;
}

static bool handle_end(const tlv_data_t *data, s_path_slice_context *context) {
    if (!tlv_get_int16(data, &context->args->end)) {
        return false;
    }
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

s_slice_bounds slice_resolve(const s_slice_args *slice, uint16_t length) {
    s_slice_bounds bounds = {
        .start = slice->has_start ? slice->start : 0,
        .end = slice->has_end ? slice->end : length,
    };

    if (bounds.start < 0) {
        bounds.start += length;
    }
    if (bounds.end < 0) {
        bounds.end += length;
    }
    return bounds;
}

bool slice_bytes(const s_slice_args *slice, uint16_t length, uint16_t *offset, uint16_t *size) {
    s_slice_bounds bounds = slice_resolve(slice, length);

    if ((bounds.start < 0) || (bounds.start >= bounds.end) || (bounds.end > length)) {
        return false;
    }
    *offset = (uint16_t) bounds.start;
    *size = (uint16_t) (bounds.end - bounds.start);
    return true;
}
