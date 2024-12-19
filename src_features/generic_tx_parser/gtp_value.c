#ifdef HAVE_GENERIC_TX_PARSER

#include "os_print.h"
#include "gtp_value.h"
#include "gtp_data_path.h"
#include "shared_context.h"  // txContext
#include "apdu_constants.h"  // APDU_RESPONSE_OK
#include "feature_signTx.h"  // get_public_key
#include "gtp_parsed_value.h"

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

// have to be declared here since it is not stored anywhere else
static uint8_t from_address[ADDRESS_LENGTH];

bool value_get(const s_value *value, s_parsed_value_collection *collection) {
    bool ret;

    switch (value->source) {
        case SOURCE_CALLDATA:
            collection->size = 0;
            ret = data_path_get(&value->data_path, collection);
            break;

        case SOURCE_RLP:
            switch (value->container_path) {
                case CP_FROM:
                    if ((ret = get_public_key(from_address, sizeof(from_address)) ==
                               APDU_RESPONSE_OK)) {
                        collection->value[0].ptr = from_address;
                        collection->value[0].length = sizeof(from_address);
                        collection->size = 1;
                    }
                    break;

                case CP_TO:
                    collection->value[0].ptr = tmpContent.txContent.destination;
                    collection->value[0].length = tmpContent.txContent.destinationLength;
                    collection->size = 1;
                    ret = true;
                    break;

                case CP_VALUE:
                    collection->value[0].ptr = tmpContent.txContent.value.value;
                    collection->value[0].length = tmpContent.txContent.value.length;
                    collection->size = 1;
                    ret = true;
                    break;

                default:
                    ret = false;
            }
            break;

        case SOURCE_CONSTANT:
            collection->value[0].ptr = value->constant.buf;
            collection->value[0].length = value->constant.size;
            collection->size = 1;
            ret = true;
            break;

        default:
            ret = false;
    }
    return ret;
}

#endif  // HAVE_GENERIC_TX_PARSER
