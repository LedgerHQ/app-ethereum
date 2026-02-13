#include "gtp_param_nft.h"
#include "manage_asset_info.h"
#include "utils.h"
#include "gtp_field_table.h"
#include "tlv_library.h"
#include "tlv_apdu.h"

#define PARAM_NFT_TAGS(X)                                    \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG) \
    X(0x01, TAG_ID, handle_id, ENFORCE_UNIQUE_TAG)           \
    X(0x02, TAG_COLLECTION, handle_collection, ENFORCE_UNIQUE_TAG)

static bool handle_version(const tlv_data_t *data, s_param_nft_context *context) {
    return tlv_get_uint8(data, &context->param->version, 0, UINT8_MAX);
}

static bool handle_id(const tlv_data_t *data, s_param_nft_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->id;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return handle_value_struct(&data->value, &ctx);
}

static bool handle_collection(const tlv_data_t *data, s_param_nft_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->collection;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return handle_value_struct(&data->value, &ctx);
}

DEFINE_TLV_PARSER(PARAM_NFT_TAGS, NULL, param_nft_tlv_parser)

bool handle_param_nft_struct(const buffer_t *buf, s_param_nft_context *context) {
    TLV_reception_t received_tags;
    return param_nft_tlv_parser(buf, context, &received_tags);
}

bool format_param_nft(const s_param_nft *param, const char *name) {
    bool ret;
    s_parsed_value_collection collections = {0};
    s_parsed_value_collection ids = {0};
    const nftInfo_t *asset;
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);
    uint8_t collection_idx;
    uint8_t addr_buf[ADDRESS_LENGTH];
    char tmp[80];

    if ((ret = value_get(&param->collection, &collections))) {
        if ((ret = value_get(&param->id, &ids))) {
            if (collections.size == 0) {
                ret = false;
            } else {
                if ((collections.size != 1) && (collections.size != ids.size)) {
                    ret = false;
                } else {
                    for (int i = 0; i < ids.size; ++i) {
                        collection_idx = (i >= collections.size) ? 0 : i;
                        buf_shrink_expand(collections.value[collection_idx].ptr,
                                          collections.value[collection_idx].length,
                                          addr_buf,
                                          sizeof(addr_buf));
                        if ((asset = (const nftInfo_t *) get_asset_info_by_addr(addr_buf)) ==
                            NULL) {
                            ret = false;
                            break;
                        }
                        if (!(ret = uint256_to_decimal(ids.value[i].ptr,
                                                       ids.value[i].length,
                                                       tmp,
                                                       sizeof(tmp)))) {
                            break;
                        }
                        snprintf(buf, buf_size, "%s #%s", asset->collectionName, tmp);
                        if (!(ret = add_to_field_table(PARAM_TYPE_NFT, name, buf, asset))) {
                            break;
                        }
                    }
                }
            }
        }
    }
    value_cleanup(&param->collection, &collections);
    value_cleanup(&param->id, &ids);
    return ret;
}
