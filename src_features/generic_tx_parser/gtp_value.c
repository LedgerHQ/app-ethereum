#ifdef HAVE_GENERIC_TX_PARSER

#include "os_print.h"
#include "gtp_value.h"
#include "gtp_data_path.h"
#include "shared_context.h" // txContext
#include "apdu_constants.h" // APDU_RESPONSE_OK
#include "feature_signTx.h" // get_public_key
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
    return true;
}

static bool handle_data_path(const s_tlv_data *data, s_value_context *context) {
    s_data_path_context ctx = {0};

    ctx.data_path = &context->value->data_path;
    if (!parse_tlv(data->value, data->length, (f_tlv_data_handler)&handle_data_path_struct, &ctx)) {
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
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    return ret;
}

static bool normalize_collection(const s_value *value, s_parsed_value_collection *collection) {
    for (int i = 0; i < collection->size; ++i) {
        switch (value->type_family) {
            case TF_ADDRESS:
                collection->value[i].ptr += (collection->value[i].length - ADDRESS_LENGTH);
                collection->value[i].length = ADDRESS_LENGTH;
                break;

            case TF_BOOL:
                collection->value[i].ptr += (collection->value[i].length - sizeof(bool));
                collection->value[i].length = sizeof(bool);
                break;

            case TF_UINT:
            case TF_INT:
            case TF_BYTES:
                if (value->type_size > 0) {
                    collection->value[i].ptr += (collection->value[i].length - value->type_size);
                    collection->value[i].length = value->type_size;
                    break;
                }
            __attribute__((fallthrough));

            case TF_STRING:
                // already to the proper size
                break;

            case TF_UFIXED:
            case TF_FIXED:
            default:
                return false;
        }
    }
    return true;
}

// have to be declared here since it is not stored anywhere else
static uint8_t from_address[ADDRESS_LENGTH];

bool value_get(const s_value *value, s_parsed_value_collection *collection) {
    bool ret;

    switch (value->source) {
        case SOURCE_CALLDATA:
            collection->size = 0;
            if ((ret = data_path_get(&value->data_path, &txContext.calldata, collection))) {
                ret = normalize_collection(value, collection);
            }
            break;

        case SOURCE_RLP:
            switch (value->container_path) {
                case CP_FROM:
                    if ((ret = get_public_key(from_address, sizeof(from_address)) == APDU_RESPONSE_OK)) {
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
        default:
            ret = false;
    }
    return ret;
}

#endif // HAVE_GENERIC_TX_PARSER
