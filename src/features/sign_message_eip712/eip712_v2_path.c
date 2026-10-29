#include <string.h>
#include "eip712_v2_path.h"
#include "os_print.h"
#include "read.h"
#include "tlv_library.h"
#include "typed_data.h"

_Static_assert(EIP712_PATH_MAX_SIZE == TD_MAX_DEPTH + 1,
               "a path has to be able to reach the deepest leaf");

// The only version of a path there is
#define EIP712_PATH_VERSION 1

// =============================================================================
// Slice
// =============================================================================

typedef struct {
    s_slice_args *args;
} s_slice_ctx;

// Bounds are fixed width: a longer encoding of a small number would read as another value
static bool get_bound(const tlv_data_t *data, int16_t *bound) {
    if (data->value.size != sizeof(*bound)) {
        return false;
    }
    *bound = (int16_t) read_u16_be(data->value.ptr, 0);
    return true;
}

static bool handle_slice_start(const tlv_data_t *data, s_slice_ctx *context) {
    context->args->has_start = true;
    return get_bound(data, &context->args->start);
}

static bool handle_slice_end(const tlv_data_t *data, s_slice_ctx *context) {
    context->args->has_end = true;
    return get_bound(data, &context->args->end);
}

#define SLICE_TAGS(X)                                                \
    X(0x01, TAG_SLICE_START, handle_slice_start, ENFORCE_UNIQUE_TAG) \
    X(0x02, TAG_SLICE_END, handle_slice_end, ENFORCE_UNIQUE_TAG)

DEFINE_TLV_PARSER(SLICE_TAGS, NULL, slice_tlv_parser)

static bool handle_slice(const tlv_data_t *data, s_slice_args *args) {
    s_slice_ctx ctx = {.args = args};
    TLV_reception_t received_tags;

    explicit_bzero(args, sizeof(*args));
    return slice_tlv_parser(&data->value, &ctx, &received_tags);
}

// =============================================================================
// Path
// =============================================================================

static s_eip712_path_element *new_element(s_eip712_path_context *context,
                                          e_eip712_path_element_type type) {
    s_eip712_path_element *element = &context->path->elements[context->path->size];

    element->type = type;
    context->path->size += 1;
    return element;
}

static bool handle_version(const tlv_data_t *data, s_eip712_path_context *context) {
    return get_uint8_t_from_tlv_data(data, &context->path->version);
}

static bool handle_struct_field(const tlv_data_t *data, s_eip712_path_context *context) {
    uint8_t index;

    if (!get_uint8_t_from_tlv_data(data, &index)) {
        return false;
    }
    new_element(context, EIP712_ELEMENT_STRUCT_FIELD)->field_index = index;
    return true;
}

static bool handle_array_slice(const tlv_data_t *data, s_eip712_path_context *context) {
    s_eip712_path_element *element = new_element(context, EIP712_ELEMENT_ARRAY_SLICE);

    return handle_slice(data, &element->slice);
}

static bool handle_leaf_slice(const tlv_data_t *data, s_eip712_path_context *context) {
    s_eip712_path_element *element = new_element(context, EIP712_ELEMENT_LEAF_SLICE);

    return handle_slice(data, &element->slice);
}

#define EIP712_PATH_TAGS(X)                                            \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG)           \
    X(0x01, TAG_STRUCT_FIELD, handle_struct_field, ALLOW_MULTIPLE_TAG) \
    X(0x02, TAG_ARRAY_SLICE, handle_array_slice, ALLOW_MULTIPLE_TAG)   \
    X(0x03, TAG_LEAF_SLICE, handle_leaf_slice, ENFORCE_UNIQUE_TAG)

static bool path_common_handler(const tlv_data_t *data, s_eip712_path_context *context);

DEFINE_TLV_PARSER(EIP712_PATH_TAGS, &path_common_handler, path_tlv_parser)

/**
 * Common handler bounding the path, and keeping the slice of the leaf last
 *
 * to be used as a TLV common handler
 */
static bool path_common_handler(const tlv_data_t *data, s_eip712_path_context *context) {
    const s_eip712_path *path = context->path;

    if (data->tag == TAG_VERSION) {
        return true;
    }
    if (path->size >= ARRAYLEN(path->elements)) {
        PRINTF("EIP712_PATH: more than %u elements\n", (unsigned int) ARRAYLEN(path->elements));
        return false;
    }
    // what follows the slice of the leaf would be steps below a leaf
    return (path->size == 0) || (path->elements[path->size - 1].type != EIP712_ELEMENT_LEAF_SLICE);
}

bool handle_eip712_path_struct(const buffer_t *buf, s_eip712_path_context *context) {
    TLV_reception_t received_tags;

    explicit_bzero(context->path, sizeof(*context->path));
    if (!path_tlv_parser(buf, context, &received_tags)) {
        return false;
    }
    if (!TLV_CHECK_RECEIVED_TAGS(received_tags, TAG_VERSION) ||
        (context->path->version != EIP712_PATH_VERSION)) {
        return false;
    }
    // a path starts from the message's root, which is a struct
    return (context->path->size > 0) &&
           (context->path->elements[0].type == EIP712_ELEMENT_STRUCT_FIELD);
}

// =============================================================================
// Resolution
// =============================================================================

// An array dimension being walked: the elements of the array still to visit
typedef struct {
    const s_struct_712_value *array;
    uint8_t element;  // position in the path of the slice selecting them
    uint16_t next;
    uint16_t end;
} s_iteration;

/**
 * Get the elements an array slice selects
 *
 * A bound counts back from the end of the array when it is negative. A selection that comes out
 * empty is valid, one that reaches outside of the array is not.
 *
 * @param[in] slice the slice
 * @param[in] size number of elements of the array
 * @param[out] start first element selected
 * @param[out] end element after the last one selected, not after @p start for an empty selection
 * @return whether the slice fits the array
 */
static bool get_array_range(const s_slice_args *slice,
                            uint16_t size,
                            uint16_t *start,
                            uint16_t *end) {
    int32_t first = slice->has_start ? slice->start : 0;
    int32_t last = slice->has_end ? slice->end : size;

    if (first < 0) {
        first += size;
    }
    if (last < 0) {
        last += size;
    }
    if ((first < 0) || (last < 0) || (last > size)) {
        return false;
    }
    *start = (uint16_t) first;
    *end = (uint16_t) last;
    return true;
}

/**
 * Narrow a value down to some of its bytes
 *
 * @param[in] slice the slice
 * @param[in,out] value the value
 * @return whether the slice fits the value, which it must leave with at least one byte
 */
static bool slice_value(const s_slice_args *slice, s_parsed_value *value) {
    int32_t first = slice->has_start ? slice->start : 0;
    int32_t last = slice->has_end ? slice->end : value->length;

    if (first < 0) {
        first += value->length;
    }
    if (last < 0) {
        last += value->length;
    }
    if ((first < 0) || (first >= last) || (last > value->length)) {
        return false;
    }
    value->ptr += first;
    value->offset += (uint16_t) first;
    value->length = (uint16_t) (last - first);
    return true;
}

/**
 * Add a leaf to the values a path leads to
 *
 * @param[in] leaf the node the path ended on
 * @param[in] slice the slice of the leaf the path ends with, NULL if none
 * @param[in,out] collection the values
 * @return whether the leaf was added
 */
static bool add_leaf(const s_struct_712_value *leaf,
                     const s_slice_args *slice,
                     s_parsed_value_collection *collection) {
    s_parsed_value *value;

    if ((leaf->kind != VAL_ATOMIC) || (collection->size >= MAX_VALUE_COLLECTION_SIZE)) {
        return false;
    }
    value = &collection->value[collection->size];
    value->ptr = leaf->data;
    value->size = leaf->length;
    value->offset = 0;
    value->length = leaf->length;
    if (slice != NULL) {
        switch (leaf->field->type) {
            case TYPE_SOL_BYTES_FIX:
            case TYPE_SOL_BYTES_DYN:
            case TYPE_SOL_STRING:
                break;
            case TYPE_STRUCT:
            case TYPE_SOL_INT:
            case TYPE_SOL_UINT:
            case TYPE_SOL_ADDRESS:
            case TYPE_SOL_BOOL:
            case TYPES_COUNT:
            default:
                return false;
        }
        if (!slice_value(slice, value)) {
            return false;
        }
    }
    collection->size += 1;
    return true;
}

bool eip712_path_get(const s_eip712_path *path, s_parsed_value_collection *collection) {
    s_iteration iterations[EIP712_PATH_MAX_SIZE];
    uint8_t depth = 0;
    const s_struct_712_value *node = td_get_message_root();
    uint8_t position = 0;

    collection->size = 0;
    if (node == NULL) {
        return false;
    }
    // every pass follows the path down to one leaf, and the next one resumes from the innermost
    // array that still has elements to visit
    for (;;) {
        const s_slice_args *leaf_slice = NULL;
        bool selected = true;

        for (; selected && (position < path->size); ++position) {
            const s_eip712_path_element *element = &path->elements[position];

            switch (element->type) {
                case EIP712_ELEMENT_STRUCT_FIELD:
                    if ((node->kind != VAL_STRUCT) ||
                        ((node = td_value_child(node, element->field_index)) == NULL)) {
                        return false;
                    }
                    break;
                case EIP712_ELEMENT_ARRAY_SLICE: {
                    uint16_t start;
                    uint16_t end;

                    if ((node->kind != VAL_ARRAY) || !get_array_range(&element->slice,
                                                                      td_value_child_count(node),
                                                                      &start,
                                                                      &end)) {
                        return false;
                    }
                    if (end <= start) {
                        selected = false;
                        break;
                    }
                    iterations[depth].array = node;
                    iterations[depth].element = position;
                    iterations[depth].next = start + 1;
                    iterations[depth].end = end;
                    depth += 1;
                    if ((node = td_value_child(node, start)) == NULL) {
                        return false;
                    }
                    break;
                }
                case EIP712_ELEMENT_LEAF_SLICE:
                    leaf_slice = &element->slice;
                    break;
                default:
                    return false;
            }
        }
        if (selected && !add_leaf(node, leaf_slice, collection)) {
            return false;
        }

        while ((depth > 0) && (iterations[depth - 1].next >= iterations[depth - 1].end)) {
            depth -= 1;
        }
        if (depth == 0) {
            return true;
        }
        node = td_value_child(iterations[depth - 1].array, iterations[depth - 1].next);
        if (node == NULL) {
            return false;
        }
        iterations[depth - 1].next += 1;
        position = iterations[depth - 1].element + 1;
    }
}
