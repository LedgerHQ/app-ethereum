#ifdef HAVE_GENERIC_TX_PARSER
#ifdef HAVE_NFT_SUPPORT

#include "gtp_param_nft.h"
#include "manage_asset_info.h"
#include "utils.h"
#include "gtp_field_table.h"

enum { TAG_VERSION = 0x00, TAG_ID = 0x01, TAG_COLLECTION = 0x02 };

static bool handle_version(const s_tlv_data *data, s_param_nft_context *context) {
    if (data->length != sizeof(context->param->version)) {
        return false;
    }
    context->param->version = data->value[0];
    return true;
}

static bool handle_id(const s_tlv_data *data, s_param_nft_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->id;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_value_struct, &ctx);
}

static bool handle_collection(const s_tlv_data *data, s_param_nft_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->collection;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_value_struct, &ctx);
}

bool handle_param_nft_struct(const s_tlv_data *data, s_param_nft_context *context) {
    bool ret;

    switch (data->tag) {
        case TAG_VERSION:
            ret = handle_version(data, context);
            break;
        case TAG_ID:
            ret = handle_id(data, context);
            break;
        case TAG_COLLECTION:
            ret = handle_collection(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    return ret;
}

bool format_param_nft(const s_param_nft *param, const char *name) {
    s_parsed_value_collection collections;
    s_parsed_value_collection ids;
    const extraInfo_t *asset;
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);
    uint8_t collection_idx;
    uint8_t addr_buf[ADDRESS_LENGTH];
    char tmp[80];

    if (!value_get(&param->collection, &collections)) {
        return false;
    }
    if (!value_get(&param->id, &ids)) {
        return false;
    }
    if (collections.size == 0) {
        return false;
    }
    if ((collections.size != 1) && (collections.size != ids.size)) {
        return false;
    }
    for (int i = 0; i < ids.size; ++i) {
        collection_idx = (i >= collections.size) ? 0 : i;
        buf_shrink_expand(collections.value[collection_idx].ptr,
                          collections.value[collection_idx].length,
                          addr_buf,
                          sizeof(addr_buf));
        if ((asset = (const extraInfo_t *) get_asset_info_by_addr(addr_buf)) == NULL) {
            return false;
        }
        if (!uint256_to_decimal(ids.value[i].ptr, ids.value[i].length, tmp, sizeof(tmp))) {
            return false;
        }
        snprintf(buf, buf_size, "%s #%s", asset->nft.collectionName, tmp);
        if (!add_to_field_table(PARAM_TYPE_NFT, name, buf)) {
            return false;
        }
    }
    return true;
}

#endif  // HAVE_NFT_SUPPORT
#endif  // HAVE_GENERIC_TX_PARSER
