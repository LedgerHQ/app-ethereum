#include "os_print.h"
#include "gcs_value.h"
#include "gtp_data_path.h"
#include "shared_context.h"  // txContext
#include "apdu_constants.h"  // SWO_SUCCESS
#include "get_public_key.h"
#include "gcs_parsed_value.h"
#include "ui_utils.h"
#include "operation_ctx.h"
#include "tlv_library.h"
#include "tlv_utils.h"
#include "map_entry.h"
#include "lcx_crc.h"

#define MAP_REF_STRUCT_VERSION 0x01
#define VALUE_STRUCT_VERSION   0x01

// =============================================================================
// MAP_REF sub-parser (nested within VALUE at tag 0x06)
// =============================================================================

typedef struct {
    s_map_ref *map_ref;
} s_map_ref_parse_ctx;

static bool handle_mr_version(const tlv_data_t *data, s_map_ref_parse_ctx *ctx) {
    (void) ctx;
    return tlv_enforce_u8_value(data, MAP_REF_STRUCT_VERSION);
}

static bool handle_mr_id(const tlv_data_t *data, s_map_ref_parse_ctx *ctx) {
    return tlv_get_uint8_range(data, &ctx->map_ref->id, 0, UINT8_MAX);
}

static bool handle_mr_key(const tlv_data_t *data, s_map_ref_parse_ctx *ctx) {
    if (data->value.size == 0 || data->value.size > sizeof(ctx->map_ref->key_tlv)) {
        PRINTF("MAP_REF KEY TLV too large: %d\n", (int) data->value.size);
        return false;
    }
    if (data->value.ptr == NULL) {
        return false;
    }
    ctx->map_ref->key_tlv_size = data->value.size;
    memcpy(ctx->map_ref->key_tlv, data->value.ptr, data->value.size);
    return true;
}

#define MAP_REF_TAGS(X)                                            \
    X(0x00, MR_TAG_VERSION, handle_mr_version, ENFORCE_UNIQUE_TAG) \
    X(0x01, MR_TAG_ID, handle_mr_id, ENFORCE_UNIQUE_TAG)           \
    X(0x02, MR_TAG_KEY, handle_mr_key, ENFORCE_UNIQUE_TAG)

DEFINE_TLV_PARSER(MAP_REF_TAGS, NULL, map_ref_tlv_parser)

static bool handle_map_ref(const tlv_data_t *data, s_value_context *context) {
    s_map_ref_parse_ctx ctx = {.map_ref = &context->value->map_ref};
    TLV_reception_t received = {0};

    explicit_bzero(&context->value->map_ref, sizeof(context->value->map_ref));
    if (!map_ref_tlv_parser(&data->value, &ctx, &received)) {
        return false;
    }
    if (!TLV_CHECK_RECEIVED_TAGS(received, MR_TAG_VERSION, MR_TAG_ID, MR_TAG_KEY)) {
        PRINTF("MAP_REF: missing mandatory tags\n");
        return false;
    }
    context->value->source = SOURCE_MAP_REF;
    return true;
}

// =============================================================================

#define VALUE_TAGS(X)                                                      \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG)               \
    X(0x01, TAG_TYPE_FAMILY, handle_type_family, ENFORCE_UNIQUE_TAG)       \
    X(0x02, TAG_TYPE_SIZE, handle_type_size, ENFORCE_UNIQUE_TAG)           \
    X(0x03, TAG_PATH, handle_path, ENFORCE_UNIQUE_TAG)                     \
    X(0x04, TAG_CONTAINER_PATH, handle_container_path, ENFORCE_UNIQUE_TAG) \
    X(0x05, TAG_CONSTANT, handle_constant, ENFORCE_UNIQUE_TAG)             \
    X(0x06, TAG_MAP_REF, handle_map_ref, ENFORCE_UNIQUE_TAG)

static bool handle_version(const tlv_data_t *data, s_value_context *context) {
    return tlv_get_uint8_range(data,
                               &context->value->version,
                               VALUE_STRUCT_VERSION,
                               VALUE_STRUCT_VERSION);
}

static bool handle_type_family(const tlv_data_t *data, s_value_context *context) {
    uint8_t tf;
    if (!tlv_get_uint8_range(data, &tf, 0, UINT8_MAX)) {
        return false;
    }
    context->value->type_family = (e_type_family) tf;
    return true;
}

static bool handle_type_size(const tlv_data_t *data, s_value_context *context) {
    return tlv_get_uint8_range(data, &context->value->type_size, 1, 32);
}

static bool handle_data_path(const tlv_data_t *data, s_value_context *context) {
    s_data_path_context ctx = {0};

    ctx.data_path = &context->value->data_path;
    explicit_bzero(ctx.data_path, sizeof(*ctx.data_path));
    if (!handle_data_path_struct(&data->value, &ctx)) {
        return false;
    }
    context->value->source = SOURCE_CALLDATA;
    return true;
}

static bool handle_eip712_path(const tlv_data_t *data, s_value_context *context) {
    s_eip712_path_context ctx = {.path = &context->value->eip712_path};

    if (!handle_eip712_path_struct(&data->value, &ctx)) {
        return false;
    }
    context->value->source = SOURCE_EIP712;
    return true;
}

// Whether the operation being described is a typed message, as opposed to a transaction
static bool is_message_operation(void) {
    const s_operation_info *info = get_current_operation_info();

    return (info != NULL) && (info->context == OPERATION_INFO_CTX_EIP712);
}

// The same tag is a path in the calldata for a transaction, and a path in the value tree for a
// message: which one it is depends on the operation the descriptor is for
static bool handle_path(const tlv_data_t *data, s_value_context *context) {
    if (is_message_operation()) {
        return handle_eip712_path(data, context);
    }
    return handle_data_path(data, context);
}

static bool handle_container_path(const tlv_data_t *data, s_value_context *context) {
    uint8_t cp;
    if (!tlv_get_uint8_range(data, &cp, 0, UINT8_MAX)) {
        return false;
    }
    context->value->container_path = (e_container_path) cp;
    context->value->source = SOURCE_RLP;
    return true;
}

static bool handle_constant(const tlv_data_t *data, s_value_context *context) {
    if (data->value.size > sizeof(context->value->constant.buf)) {
        return false;
    }
    context->value->constant.size = data->value.size;
    if (data->value.size > 0) {
        if (data->value.ptr == NULL) {
            return false;
        }
        memcpy(context->value->constant.buf, data->value.ptr, data->value.size);
    }
    context->value->source = SOURCE_CONSTANT;
    return true;
}

DEFINE_TLV_PARSER(VALUE_TAGS, NULL, value_tlv_parser)

static size_t count_received_sources(TLV_reception_t received) {
    const TLV_tag_t sources[] = {TAG_PATH, TAG_CONTAINER_PATH, TAG_CONSTANT, TAG_MAP_REF};
    size_t count = 0;

    for (size_t i = 0; i < sizeof(sources) / sizeof(sources[0]); i++) {
        if (tlv_check_received_tags(received, &sources[i], 1)) {
            count++;
        }
    }
    return count;
}

bool handle_value_struct(const buffer_t *buf, s_value_context *context) {
    TLV_reception_t received_tags;

    if (!value_tlv_parser(buf, context, &received_tags)) {
        return false;
    }
    if (!TLV_CHECK_RECEIVED_TAGS(received_tags, TAG_VERSION, TAG_TYPE_FAMILY)) {
        PRINTF("VALUE: missing mandatory tags\n");
        return false;
    }
    // Each source tag sets the source: with several of them the last one received would win
    if (count_received_sources(received_tags) != 1) {
        PRINTF("VALUE: exactly one source is required\n");
        return false;
    }
    return true;
}

// SOURCE_RLP: read a value directly from a well-known RLP field of the current tx.
static bool value_get_rlp(const s_value *value, s_parsed_value_collection *collection) {
    // Held static because its address is exposed to the caller via the
    // collection and must remain valid after this function returns.
    static uint64_t chain_id = 0;

    switch (value->container_path) {
        // Sender address (20 bytes)
        case CP_FROM:
            if ((collection->value[0].ptr = get_current_operation_from()) == NULL) {
                return false;
            }
            collection->value[0].length = ADDRESS_LENGTH;
            collection->size = 1;
            break;

        // Recipient address (20 bytes)
        case CP_TO:
            if ((collection->value[0].ptr = get_current_operation_to()) == NULL) {
                return false;
            }
            collection->value[0].length = ADDRESS_LENGTH;
            collection->size = 1;
            break;

        // Native amount transferred (256-bit big-endian)
        case CP_VALUE:
            if ((collection->value[0].ptr = get_current_operation_amount()) == NULL) {
                return false;
            }
            collection->value[0].length = INT256_LENGTH;
            collection->size = 1;
            break;

        // Chain ID, normalized to a big-endian 64-bit value
        case CP_CHAIN_ID:
            if (!get_current_operation_chain_id(&chain_id)) {
                return false;
            }
            // Convert to big-endian byte array
            chain_id = u64_from_BE((const uint8_t *) &chain_id, sizeof(uint64_t));
            collection->value[0].ptr = (const uint8_t *) &chain_id;
            collection->value[0].length = sizeof(uint64_t);
            collection->size = 1;
            break;

        default:
            return false;
    }
    return true;
}

// SOURCE_MAP_REF: resolve a key value, then fetch the matching map entry.
static bool value_get_map_ref(const s_value *value, s_parsed_value_collection *collection) {
    // The key to look up is itself a nested value descriptor (TLV);
    // parse it into a temporary value before resolving it.
    s_value key_value = {0};
    s_value_context key_ctx = {.value = &key_value};
    s_parsed_value_collection key_collection = {0};
    const s_map_entry *entry;
    buffer_t key_buf = {.ptr = (uint8_t *) value->map_ref.key_tlv,
                        .size = value->map_ref.key_tlv_size,
                        .offset = 0};

    // Decode the embedded key descriptor
    if (!handle_value_struct(&key_buf, &key_ctx)) {
        return false;
    }
    // Refuse recursion: a map key cannot itself be a map reference
    if (key_value.source == SOURCE_MAP_REF) {
        PRINTF("Error: Nested MAP_REF not supported\n");
        return false;
    }
    // Resolve the key descriptor to its concrete byte value
    if (!value_get(&key_value, &key_collection)) {
        // A failing data path may have already allocated some leaves
        value_cleanup(&key_value, &key_collection);
        return false;
    }
    // A map key must resolve to exactly one value fitting in a uint8 length
    if (key_collection.size != 1 || key_collection.value[0].length > UINT8_MAX) {
        value_cleanup(&key_value, &key_collection);
        return false;
    }
    // Look up the map entry matching (map id, key), then release the key
    entry = get_matching_map_entry(value->map_ref.id,
                                   key_collection.value[0].ptr,
                                   (uint8_t) key_collection.value[0].length);
    value_cleanup(&key_value, &key_collection);
    if (entry == NULL) {
        PRINTF("Error: No MAP_ENTRY found for id=%d\n", (int) value->map_ref.id);
        return false;
    }
    // Expose the resolved entry's value to the caller
    collection->value[0].ptr = entry->value;
    collection->value[0].length = entry->value_size;
    collection->size = 1;
    return true;
}

bool value_get(const s_value *value, s_parsed_value_collection *collection) {
    switch (value->source) {
        // Value extracted from the transaction calldata via a data path
        case SOURCE_CALLDATA:
            if (is_message_operation()) {
                return false;
            }
            collection->size = 0;
            if (!data_path_get(&value->data_path, collection)) {
                return false;
            }
            break;

        // Value extracted from the message's value tree via a path
        case SOURCE_EIP712:
            if (!is_message_operation() || !eip712_path_get(&value->eip712_path, collection)) {
                return false;
            }
            break;

        // Value read directly from a well-known RLP field of the current tx
        case SOURCE_RLP:
            if (!value_get_rlp(value, collection)) {
                return false;
            }
            break;

        // Literal constant carried by the descriptor itself
        case SOURCE_CONSTANT:
            collection->value[0].ptr = value->constant.buf;
            collection->value[0].length = value->constant.size;
            collection->size = 1;
            break;

        // Indirect lookup: resolve a key value, then fetch the matching map entry
        case SOURCE_MAP_REF:
            if (!value_get_map_ref(value, collection)) {
                return false;
            }
            break;

        default:
            return false;
    }
    return true;
}

uint32_t value_source_key(const s_value *value) {
    uint32_t key = cx_crc32(&value->source, sizeof(value->source));

    switch (value->source) {
        case SOURCE_CALLDATA:
            return cx_crc32_update(key, &value->data_path, sizeof(value->data_path));
        case SOURCE_EIP712:
            return cx_crc32_update(key, &value->eip712_path, sizeof(value->eip712_path));
        case SOURCE_RLP:
            return cx_crc32_update(key, &value->container_path, sizeof(value->container_path));
        case SOURCE_CONSTANT:
            key = cx_crc32_update(key, &value->constant.size, sizeof(value->constant.size));
            return cx_crc32_update(key, value->constant.buf, value->constant.size);
        case SOURCE_MAP_REF:
            return cx_crc32_update(key, &value->map_ref, sizeof(value->map_ref));
        default:
            return key;
    }
}

static void set_shown(const s_value *value, uint8_t index, uint8_t as) {
    uint8_t what = 0;

    if (value->source == SOURCE_RLP) {
        switch (value->container_path) {
            case CP_FROM:
                what = OPERATION_SHOWN_FROM;
                break;
            case CP_TO:
                what = OPERATION_SHOWN_TO;
                break;
            case CP_VALUE:
                what = OPERATION_SHOWN_VALUE;
                break;
            case CP_CHAIN_ID:
                what = OPERATION_SHOWN_CHAIN_ID;
                break;
            default:
                break;
        }
    }
    operation_ctx_set_shown(what, as, value_source_key(value), index);
}

void value_set_shown(const s_value *value, uint8_t index) {
    set_shown(value, index, OPERATION_SHOWN_ALL);
}

void value_set_selector_shown(const s_value *value, uint8_t index) {
    set_shown(value, index, OPERATION_SHOWN_TO);
}

void value_cleanup(const s_value *value, const s_parsed_value_collection *collection) {
    if (value->source == SOURCE_CALLDATA) {
        data_path_cleanup(collection);
    }
}
