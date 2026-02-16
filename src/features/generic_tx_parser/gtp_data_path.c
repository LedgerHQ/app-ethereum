#include <string.h>  // memcpy / explicit_bzero
#include "os_print.h"
#include "os_math.h"  // MIN
#include "gtp_data_path.h"
#include "read.h"
#include "utils.h"
#include "calldata.h"
#include "app_mem_utils.h"
#include "tx_ctx.h"
#include "tlv_library.h"
#include "tlv_apdu.h"

#define DATA_PATH_TAGS(X)                                    \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG) \
    X(0x01, TAG_TUPLE, handle_tuple, ALLOW_MULTIPLE_TAG)     \
    X(0x02, TAG_ARRAY, handle_array, ALLOW_MULTIPLE_TAG)     \
    X(0x03, TAG_REF, handle_ref, ALLOW_MULTIPLE_TAG)         \
    X(0x04, TAG_LEAF, handle_leaf, ALLOW_MULTIPLE_TAG)       \
    X(0x05, TAG_SLICE, handle_slice, ALLOW_MULTIPLE_TAG)

static bool handle_version(const tlv_data_t *data, s_data_path_context *context) {
    return tlv_get_uint8(data, &context->data_path->version, 0, UINT8_MAX);
}

static bool handle_tuple(const tlv_data_t *data, s_data_path_context *context) {
    if (!tlv_get_uint16(data,
                        &context->data_path->elements[context->data_path->size].tuple.value,
                        0,
                        UINT16_MAX)) {
        return false;
    }
    context->data_path->elements[context->data_path->size].type = ELEMENT_TYPE_TUPLE;
    context->data_path->size += 1;
    return true;
}

static bool handle_array(const tlv_data_t *data, s_data_path_context *context) {
    s_path_array_context ctx = {0};

    ctx.args = &context->data_path->elements[context->data_path->size].array;
    explicit_bzero(ctx.args, sizeof(*ctx.args));
    if (!handle_array_struct(&data->value, &ctx)) {
        return false;
    }
    context->data_path->elements[context->data_path->size].type = ELEMENT_TYPE_ARRAY;
    context->data_path->size += 1;
    return true;
}

static bool handle_ref(const tlv_data_t *data, s_data_path_context *context) {
    if (data->value.size != 0) {
        return false;
    }
    context->data_path->elements[context->data_path->size].type = ELEMENT_TYPE_REF;
    context->data_path->size += 1;
    return true;
}

static bool handle_leaf(const tlv_data_t *data, s_data_path_context *context) {
    if (!tlv_get_uint8(data,
                       &context->data_path->elements[context->data_path->size].leaf.type,
                       0,
                       UINT8_MAX)) {
        return false;
    }
    context->data_path->elements[context->data_path->size].type = ELEMENT_TYPE_LEAF;
    context->data_path->size += 1;
    return true;
}

static bool handle_slice(const tlv_data_t *data, s_data_path_context *context) {
    s_path_slice_context ctx = {0};

    ctx.args = &context->data_path->elements[context->data_path->size].slice;
    explicit_bzero(ctx.args, sizeof(*ctx.args));
    if (!handle_slice_struct(&data->value, &ctx)) {
        return false;
    }
    context->data_path->elements[context->data_path->size].type = ELEMENT_TYPE_SLICE;
    context->data_path->size += 1;
    return true;
}

static bool data_path_common_handler(const tlv_data_t *data, s_data_path_context *context);

DEFINE_TLV_PARSER(DATA_PATH_TAGS, &data_path_common_handler, data_path_tlv_parser)

// Common handler to check path size limits
static bool data_path_common_handler(const tlv_data_t *data, s_data_path_context *context) {
    // Check size limit for non-VERSION tags
    if (data->tag != TAG_VERSION) {
        if (context->data_path->size >= PATH_MAX_SIZE) {
            PRINTF("Error: PATH_MAX_SIZE exceeded\n");
            return false;
        }
    }
    return true;
}

bool handle_data_path_struct(const buffer_t *buf, s_data_path_context *context) {
    TLV_reception_t received_tags;
    return data_path_tlv_parser(buf, context, &received_tags);
}

static bool path_tuple(const s_tuple_args *tuple, uint32_t *offset, uint32_t *ref_offset) {
    *ref_offset = *offset;
    *offset += tuple->value;
    return true;
}

static bool path_ref(uint32_t *offset, uint32_t *ref_offset) {
    uint8_t buf[sizeof(uint16_t)];
    const uint8_t *chunk;

    if ((chunk = calldata_get_chunk(get_current_calldata(), *offset)) == NULL) {
        return false;
    }
    buf_shrink_expand(chunk, CALLDATA_CHUNK_SIZE, buf, sizeof(buf));
    *offset = read_u16_be(buf, 0) / CALLDATA_CHUNK_SIZE;
    *offset += *ref_offset;
    return true;
}

static bool path_leaf(const s_leaf_args *leaf,
                      uint32_t *offset,
                      s_parsed_value_collection *collection) {
    uint8_t buf[sizeof(uint16_t)];
    const uint8_t *chunk;
    uint8_t *leaf_buf = NULL;
    uint8_t cpy_length;

    if (collection->size > MAX_VALUE_COLLECTION_SIZE) {
        return false;
    }

    switch (leaf->type) {
        case LEAF_TYPE_STATIC:
            collection->value[collection->size].size = CALLDATA_CHUNK_SIZE;
            break;

        case LEAF_TYPE_DYNAMIC:
            if ((chunk = calldata_get_chunk(get_current_calldata(), *offset)) == NULL) {
                return false;
            }
            buf_shrink_expand(chunk, CALLDATA_CHUNK_SIZE, buf, sizeof(buf));
            collection->value[collection->size].size = read_u16_be(buf, 0);
            *offset += 1;
            break;

        default:
            return false;
    }
    collection->value[collection->size].length = collection->value[collection->size].size;
    collection->value[collection->size].offset = 0;
    if (collection->value[collection->size].length > 0) {
        if ((leaf_buf = APP_MEM_ALLOC(collection->value[collection->size].length)) == NULL) {
            return false;
        }
        for (int chunk_idx = 0;
             (chunk_idx * CALLDATA_CHUNK_SIZE) < collection->value[collection->size].length;
             ++chunk_idx) {
            if ((chunk = calldata_get_chunk(get_current_calldata(), *offset + chunk_idx)) == NULL) {
                APP_MEM_FREE(leaf_buf);
                return false;
            }
            cpy_length =
                MIN(CALLDATA_CHUNK_SIZE,
                    collection->value[collection->size].length - (chunk_idx * CALLDATA_CHUNK_SIZE));
            memcpy(leaf_buf + (chunk_idx * CALLDATA_CHUNK_SIZE), chunk, cpy_length);
        }
    }
    collection->value[collection->size].ptr = leaf_buf;
    collection->size += 1;
    return true;
}

static bool path_slice(const s_slice_args *slice, s_parsed_value_collection *collection) {
    uint16_t start;
    uint16_t end;
    uint16_t value_length;

    if (collection->size == 0) {
        return false;
    }

    value_length = collection->value[collection->size - 1].length;
    if (slice->has_start) {
        start = (slice->start < 0) ? ((int16_t) value_length + slice->start) : slice->start;
    } else {
        start = 0;
    }

    if (slice->has_end) {
        end = (slice->end < 0) ? ((int16_t) value_length + slice->end) : slice->end;
    } else {
        end = value_length;
    }

    if ((start >= end) || (end > value_length)) {
        return false;
    }
    collection->value[collection->size - 1].ptr += start;
    collection->value[collection->size - 1].length = (end - start);
    collection->value[collection->size - 1].offset += start;
    return true;
}

#define MAX_ARRAYS 8

typedef struct {
    uint8_t depth;
    uint8_t passes_remaining[MAX_ARRAYS];
    uint8_t index;
} s_arrays_info;

static bool path_array(const s_array_args *array,
                       uint32_t *offset,
                       uint32_t *ref_offset,
                       s_arrays_info *arrays_info) {
    uint8_t buf[sizeof(uint16_t)];
    uint16_t array_size;
    uint16_t idx;
    uint16_t start;
    uint16_t end;
    const uint8_t *chunk;

    if (arrays_info->index >= MAX_ARRAYS) {
        return false;
    }
    if ((chunk = calldata_get_chunk(get_current_calldata(), *offset)) == NULL) {
        return false;
    }
    buf_shrink_expand(chunk, CALLDATA_CHUNK_SIZE, buf, sizeof(buf));
    array_size = read_u16_be(buf, 0);

    if (array->has_start) {
        start = (array->start < 0) ? ((int16_t) array_size + array->start) : array->start;
    } else {
        start = 0;
    }

    if (array->has_end) {
        end = (array->end < 0) ? ((int16_t) array_size + array->end) : array->end;
    } else {
        end = array_size;
    }

    *offset += 1;
    if (arrays_info->index == arrays_info->depth) {
        // new depth
        arrays_info->passes_remaining[arrays_info->index] = (end - start);
        arrays_info->depth += 1;
    }
    idx = start + ((end - start) - arrays_info->passes_remaining[arrays_info->index]);
    *ref_offset = *offset;
    *offset += (idx * array->weight);
    arrays_info->index += 1;
    return true;
}

static void arrays_update(s_arrays_info *arrays_info) {
    while (arrays_info->depth > 0) {
        if ((arrays_info->passes_remaining[arrays_info->depth - 1] -= 1) > 0) {
            break;
        }
        arrays_info->depth -= 1;
    }
}

bool data_path_get(const s_data_path *data_path, s_parsed_value_collection *collection) {
    bool ret;
    uint32_t offset;
    uint32_t ref_offset;
    s_arrays_info arinf = {0};

    do {
        arinf.index = 0;
        offset = 0;
        ref_offset = offset;
        for (int i = 0; i < data_path->size; ++i) {
            switch (data_path->elements[i].type) {
                case ELEMENT_TYPE_TUPLE:
                    ret = path_tuple(&data_path->elements[i].tuple, &offset, &ref_offset);
                    break;

                case ELEMENT_TYPE_ARRAY:
                    ret = path_array(&data_path->elements[i].array, &offset, &ref_offset, &arinf);
                    break;

                case ELEMENT_TYPE_REF:
                    ret = path_ref(&offset, &ref_offset);
                    break;

                case ELEMENT_TYPE_LEAF:
                    ret = path_leaf(&data_path->elements[i].leaf, &offset, collection);
                    break;

                case ELEMENT_TYPE_SLICE:
                    ret = path_slice(&data_path->elements[i].slice, collection);
                    break;

                default:
                    ret = false;
            }

            if (!ret) return false;
        }
        arrays_update(&arinf);
    } while (arinf.depth > 0);
    return true;
}

void data_path_cleanup(const s_parsed_value_collection *collection) {
    for (int i = 0; i < collection->size; ++i) {
        if (collection->value[i].ptr != NULL) {
            APP_MEM_FREE((void *) collection->value[i].ptr - collection->value[i].offset);
        }
    }
}
