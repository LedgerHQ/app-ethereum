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

enum {
    TAG_VERSION = 0x00,
    TAG_TYPE_FAMILY = 0x01,
    TAG_TYPE_SIZE = 0x02,
    TAG_DATA_PATH = 0x03,
    TAG_CONTAINER_PATH = 0x04,
    TAG_CONSTANT = 0x05,
};

static bool handle_version(const s_tlv_data *data, s_value_context *context) {
    if (data->length != sizeof(uint8_t)) {
        return false;
    }
    context->value->version = data->value[0];
    return true;
}

static bool handle_type_family(const s_tlv_data *data, s_value_context *context) {
    if (data->length != sizeof(e_type_family)) {
        return false;
    }
    context->value->type_family = data->value[0];
    return true;
}

static bool handle_type_size(const s_tlv_data *data, s_value_context *context) {
    if (data->length != sizeof(uint8_t)) {
        return false;
    }
    context->value->type_size = data->value[0];
    return (context->value->type_size > 0) && (context->value->type_size <= 32);
}

static bool handle_data_path(const s_tlv_data *data, s_value_context *context) {
    s_data_path_context ctx = {0};

    ctx.data_path = &context->value->data_path;
    explicit_bzero(ctx.data_path, sizeof(*ctx.data_path));
    if (!tlv_parse(data->value,
                   data->length,
                   (f_tlv_data_handler) &handle_data_path_struct,
                   &ctx)) {
        return false;
    }
    context->value->source = SOURCE_CALLDATA;
    return true;
}

static bool handle_container_path(const s_tlv_data *data, s_value_context *context) {
    if (data->length != sizeof(e_container_path)) {
        return false;
    }
    context->value->container_path = data->value[0];
    context->value->source = SOURCE_RLP;
    return true;
}

static bool handle_constant(const s_tlv_data *data, s_value_context *context) {
    if (data->length > sizeof(context->value->constant.buf)) {
        return false;
    }
    context->value->constant.size = data->length;
    memcpy(context->value->constant.buf, data->value, data->length);
    context->value->source = SOURCE_CONSTANT;
    return true;
}

bool handle_value_struct(const s_tlv_data *data, s_value_context *context) {
    bool ret;

    switch (data->tag) {
        case TAG_VERSION:
            ret = handle_version(data, context);
            break;
        case TAG_TYPE_FAMILY:
            ret = handle_type_family(data, context);
            break;
        case TAG_TYPE_SIZE:
            ret = handle_type_size(data, context);
            break;
        case TAG_DATA_PATH:
            ret = handle_data_path(data, context);
            break;
        case TAG_CONTAINER_PATH:
            ret = handle_container_path(data, context);
            break;
        case TAG_CONSTANT:
            ret = handle_constant(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    return ret;
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
