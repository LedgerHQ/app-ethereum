#ifdef HAVE_GENERIC_TX_PARSER

#include <string.h>  // memcpy / explicit_bzero
#include "os_print.h"
#include "gtp_data_path.h"
#include "read.h"
#include "utils.h"
#include "calldata.h"

enum {
    TAG_VERSION = 0x00,
    TAG_TUPLE = 0x01,
    TAG_ARRAY = 0x02,
    TAG_REF = 0x03,
    TAG_LEAF = 0x04,
    TAG_SLICE = 0x05,
};

static bool handle_version(const s_tlv_data *data, s_data_path_context *context) {
    if (data->length != sizeof(context->data_path->version)) {
        return false;
    }
    context->data_path->version = data->value[0];
    return true;
}

static bool handle_tuple(const s_tlv_data *data, s_data_path_context *context) {
    uint8_t buf[sizeof(uint16_t)];

    if (data->length > sizeof(buf)) {
        return false;
    }
    buf_shrink_expand(data->value, data->length, buf, sizeof(buf));
    context->data_path->elements[context->data_path->size].type = ELEMENT_TYPE_TUPLE;
    context->data_path->elements[context->data_path->size].tuple.value = read_u16_be(buf, 0);

    context->data_path->size += 1;
    return true;
}

static bool handle_array(const s_tlv_data *data, s_data_path_context *context) {
    s_path_array_context ctx = {0};

    ctx.args = &context->data_path->elements[context->data_path->size].array;
    explicit_bzero(ctx.args, sizeof(*ctx.args));
    if (!tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_array_struct, &ctx))
        return false;
    context->data_path->elements[context->data_path->size].type = ELEMENT_TYPE_ARRAY;
    context->data_path->size += 1;
    return true;
}

static bool handle_ref(const s_tlv_data *data, s_data_path_context *context) {
    if (data->length != 0) {
        return false;
    }
    context->data_path->elements[context->data_path->size].type = ELEMENT_TYPE_REF;

    context->data_path->size += 1;
    return true;
}

static bool handle_leaf(const s_tlv_data *data, s_data_path_context *context) {
    if (data->length != sizeof(e_path_leaf_type)) {
        return false;
    }
    context->data_path->elements[context->data_path->size].type = ELEMENT_TYPE_LEAF;
    context->data_path->elements[context->data_path->size].leaf.type = data->value[0];

    context->data_path->size += 1;
    return true;
}

static bool handle_slice(const s_tlv_data *data, s_data_path_context *context) {
    s_path_slice_context ctx = {0};

    ctx.args = &context->data_path->elements[context->data_path->size].slice;
    explicit_bzero(ctx.args, sizeof(*ctx.args));
    if (!tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_slice_struct, &ctx))
        return false;
    context->data_path->elements[context->data_path->size].type = ELEMENT_TYPE_SLICE;
    context->data_path->size += 1;
    return true;
}

bool handle_data_path_struct(const s_tlv_data *data, s_data_path_context *context) {
    bool ret;

    if (data->tag != TAG_VERSION) {
        if (context->data_path->size >= PATH_MAX_SIZE) {
            return false;
        }
    }
    switch (data->tag) {
        case TAG_VERSION:
            ret = handle_version(data, context);
            break;
        case TAG_TUPLE:
            ret = handle_tuple(data, context);
            break;
        case TAG_ARRAY:
            ret = handle_array(data, context);
            break;
        case TAG_REF:
            ret = handle_ref(data, context);
            break;
        case TAG_LEAF:
            ret = handle_leaf(data, context);
            break;
        case TAG_SLICE:
            ret = handle_slice(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    return ret;
}

static bool path_tuple(const s_tuple_args *tuple, uint32_t *offset, uint32_t *ref_offset) {
    *ref_offset = *offset;
    *offset += tuple->value;
    return true;
}

static bool path_ref(uint32_t *offset, uint32_t *ref_offset) {
    uint8_t buf[sizeof(uint16_t)];
    const uint8_t *chunk;

    if ((chunk = calldata_get_chunk(*offset)) == NULL) {
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

    if (collection->size > MAX_VALUE_COLLECTION_SIZE) {
        return false;
    }

    switch (leaf->type) {
        case LEAF_TYPE_STATIC:
            collection->value[collection->size].length = CALLDATA_CHUNK_SIZE;
            break;

        case LEAF_TYPE_DYNAMIC:
            if ((chunk = calldata_get_chunk(*offset)) == NULL) {
                return false;
            }
            // TODO: properly handle multi-chunk dynamic values once calldata compression
            // is implemented
            buf_shrink_expand(chunk, CALLDATA_CHUNK_SIZE, buf, sizeof(buf));
            collection->value[collection->size].length = read_u16_be(buf, 0);
            *offset += 1;
            break;

        default:
            return false;
    }
    if ((chunk = calldata_get_chunk(*offset)) == NULL) {
        return false;
    }
    collection->value[collection->size].ptr = chunk;
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
    if ((chunk = calldata_get_chunk(*offset)) == NULL) {
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

#endif  // HAVE_GENERIC_TX_PARSER
