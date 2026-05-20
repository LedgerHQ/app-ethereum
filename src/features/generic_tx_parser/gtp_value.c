#include "os_print.h"
#include "gtp_value.h"
#include "gtp_data_path.h"
#include "shared_context.h"  // txContext
#include "apdu_constants.h"  // SWO_SUCCESS
#include "get_public_key.h"
#include "gtp_parsed_value.h"
#include "ui_utils.h"
#include "tx_ctx.h"
#include "tlv_library.h"
#include "tlv_utils.h"
#include "map_entry.h"

// =============================================================================
// MAP_REF sub-parser (nested within VALUE at tag 0x06)
// =============================================================================

typedef struct {
    s_map_ref *map_ref;
} s_map_ref_parse_ctx;

static bool handle_mr_version(const tlv_data_t *data, s_map_ref_parse_ctx *ctx) {
    uint8_t v;
    (void) ctx;
    return tlv_get_uint8_range(data, &v, 0, UINT8_MAX);
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
    X(0x03, TAG_DATA_PATH, handle_data_path, ENFORCE_UNIQUE_TAG)           \
    X(0x04, TAG_CONTAINER_PATH, handle_container_path, ENFORCE_UNIQUE_TAG) \
    X(0x05, TAG_CONSTANT, handle_constant, ENFORCE_UNIQUE_TAG)             \
    X(0x06, TAG_MAP_REF, handle_map_ref, ENFORCE_UNIQUE_TAG)

static bool handle_version(const tlv_data_t *data, s_value_context *context) {
    return tlv_get_uint8_range(data, &context->value->version, 0, UINT8_MAX);
}

static bool handle_type_family(const tlv_data_t *data, s_value_context *context) {
    return tlv_get_uint8_range(data, (uint8_t *) &context->value->type_family, 0, UINT8_MAX);
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

static bool handle_container_path(const tlv_data_t *data, s_value_context *context) {
    if (!tlv_get_uint8_range(data, (uint8_t *) &context->value->container_path, 0, UINT8_MAX)) {
        return false;
    }
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

bool handle_value_struct(const buffer_t *buf, s_value_context *context) {
    TLV_reception_t received_tags;
    return value_tlv_parser(buf, context, &received_tags);
}

bool value_get(const s_value *value, s_parsed_value_collection *collection) {
    static uint64_t chain_id = 0;
    const s_tx_info *tx_info = NULL;

    switch (value->source) {
        case SOURCE_CALLDATA:
            collection->size = 0;
            if (!data_path_get(&value->data_path, collection)) {
                return false;
            }
            break;

        case SOURCE_RLP:
            switch (value->container_path) {
                case CP_FROM:
                    if (((collection->value[0].ptr = get_current_tx_from())) == NULL) {
                        return false;
                    }
                    collection->value[0].length = ADDRESS_LENGTH;
                    collection->size = 1;
                    break;

                case CP_TO:
                    if ((collection->value[0].ptr = get_current_tx_to()) == NULL) {
                        return false;
                    }
                    collection->value[0].length = ADDRESS_LENGTH;
                    collection->size = 1;
                    break;

                case CP_VALUE:
                    if ((collection->value[0].ptr = get_current_tx_amount()) == NULL) {
                        return false;
                    }
                    collection->value[0].length = INT256_LENGTH;
                    collection->size = 1;
                    break;

                case CP_CHAIN_ID:
                    // Get chain ID as uint64_t
                    tx_info = get_current_tx_info();
                    if (tx_info == NULL) {
                        return false;
                    }
                    chain_id = tx_info->chain_id;
                    // Convert to big-endian byte array
                    chain_id = u64_from_BE((const uint8_t *) &chain_id, sizeof(uint64_t));
                    collection->value[0].ptr = (const uint8_t *) &chain_id;
                    collection->value[0].length = sizeof(uint64_t);
                    collection->size = 1;
                    break;

                default:
                    return false;
            }
            break;

        case SOURCE_CONSTANT:
            collection->value[0].ptr = value->constant.buf;
            collection->value[0].length = value->constant.size;
            collection->size = 1;
            break;

        case SOURCE_MAP_REF: {
            s_value key_value = {0};
            s_value_context key_ctx = {.value = &key_value};
            s_parsed_value_collection key_collection = {0};
            const s_map_entry *entry;
            buffer_t key_buf = {.ptr = (uint8_t *) value->map_ref.key_tlv,
                                .size = value->map_ref.key_tlv_size,
                                .offset = 0};

            if (!handle_value_struct(&key_buf, &key_ctx)) {
                return false;
            }
            if (key_value.source == SOURCE_MAP_REF) {
                PRINTF("Error: Nested MAP_REF not supported\n");
                return false;
            }
            if (!value_get(&key_value, &key_collection)) {
                return false;
            }
            if (key_collection.size != 1 || key_collection.value[0].length > UINT8_MAX) {
                value_cleanup(&key_value, &key_collection);
                return false;
            }
            entry = get_matching_map_entry(value->map_ref.id,
                                           key_collection.value[0].ptr,
                                           (uint8_t) key_collection.value[0].length);
            value_cleanup(&key_value, &key_collection);
            if (entry == NULL) {
                PRINTF("Error: No MAP_ENTRY found for id=%d\n", (int) value->map_ref.id);
                return false;
            }
            collection->value[0].ptr = entry->value;
            collection->value[0].length = entry->value_size;
            collection->size = 1;
            break;
        }

        default:
            return false;
    }
    return true;
}

void value_cleanup(const s_value *value, const s_parsed_value_collection *collection) {
    if (value->source == SOURCE_CALLDATA) {
        data_path_cleanup(collection);
    }
}
