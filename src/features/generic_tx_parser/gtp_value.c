#include "os_print.h"
#include "gtp_value.h"
#include "gtp_data_path.h"
#include "shared_context.h"  // txContext
#include "apdu_constants.h"  // SWO_SUCCESS
#include "getPublicKey.h"
#include "gtp_parsed_value.h"
#include "mem.h"
#include "mem_utils.h"
#include "ui_utils.h"
#include "tx_ctx.h"
#include "tlv_library.h"
#include "tlv_apdu.h"

#define VALUE_TAGS(X)                                                      \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG)               \
    X(0x01, TAG_TYPE_FAMILY, handle_type_family, ENFORCE_UNIQUE_TAG)       \
    X(0x02, TAG_TYPE_SIZE, handle_type_size, ENFORCE_UNIQUE_TAG)           \
    X(0x03, TAG_DATA_PATH, handle_data_path, ENFORCE_UNIQUE_TAG)           \
    X(0x04, TAG_CONTAINER_PATH, handle_container_path, ENFORCE_UNIQUE_TAG) \
    X(0x05, TAG_CONSTANT, handle_constant, ENFORCE_UNIQUE_TAG)

static bool handle_version(const tlv_data_t *data, s_value_context *context) {
    return tlv_get_uint8(data, &context->value->version, 0, UINT8_MAX);
}

static bool handle_type_family(const tlv_data_t *data, s_value_context *context) {
    return tlv_get_uint8(data, (uint8_t *) &context->value->type_family, 0, UINT8_MAX);
}

static bool handle_type_size(const tlv_data_t *data, s_value_context *context) {
    return tlv_get_uint8(data, &context->value->type_size, 1, 32);
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
    if (!tlv_get_uint8(data, (uint8_t *) &context->value->container_path, 0, UINT8_MAX)) {
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
    memcpy(context->value->constant.buf, data->value.ptr, data->value.size);
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
