#include "auth_7702.h"
#include "utils.h"
#include "read.h"

enum {
    TAG_STRUCT_VERSION = 0x00,
    TAG_DELEGATE_ADDR = 0x01,
    TAG_CHAIN_ID = 0x02,
    TAG_NONCE = 0x03,
};

enum {
    BIT_VERSION,
    BIT_DELEGATE_ADDR,
    BIT_CHAIN_ID,
    BIT_NONCE,
};

#define STRUCT_VERSION 0x01
#define MASK_ALL \
    (SET_BIT(BIT_VERSION) | SET_BIT(BIT_DELEGATE_ADDR) | SET_BIT(BIT_CHAIN_ID) | SET_BIT(BIT_NONCE))

static bool handle_version(const s_tlv_data *data, s_auth_7702_ctx *context) {
    if (data->length != sizeof(uint8_t)) {
        return false;
    }
    context->mask_parsed |= SET_BIT(BIT_VERSION);
    context->version = data->value[0];
    return true;
}

static bool handle_delegate_addr(const s_tlv_data *data, s_auth_7702_ctx *context) {
    uint8_t buf[sizeof(context->auth_7702.delegate)];

    if (data->length > sizeof(buf)) {
        return false;
    }
    buf_shrink_expand(data->value, data->length, buf, sizeof(buf));
    memmove(context->auth_7702.delegate, buf, sizeof(buf));
    context->mask_parsed |= SET_BIT(BIT_DELEGATE_ADDR);
    return true;
}

static bool handle_chain_id(const s_tlv_data *data, s_auth_7702_ctx *context) {
    uint8_t buf[sizeof(context->auth_7702.chainId)];

    if (data->length > sizeof(buf)) {
        return false;
    }
    buf_shrink_expand(data->value, data->length, buf, sizeof(buf));
    context->auth_7702.chainId = read_u64_be(buf, 0);
    context->mask_parsed |= SET_BIT(BIT_CHAIN_ID);
    return true;
}

static bool handle_nonce(const s_tlv_data *data, s_auth_7702_ctx *context) {
    uint8_t buf[sizeof(context->auth_7702.nonce)];

    if (data->length > sizeof(buf)) {
        return false;
    }
    buf_shrink_expand(data->value, data->length, buf, sizeof(buf));
    context->auth_7702.nonce = read_u64_be(buf, 0);
    context->mask_parsed |= SET_BIT(BIT_NONCE);
    return true;
}

bool handle_auth_7702_struct(const s_tlv_data *data, s_auth_7702_ctx *context) {
    bool ret;

    switch (data->tag) {
        case TAG_STRUCT_VERSION:
            ret = handle_version(data, context);
            break;
        case TAG_DELEGATE_ADDR:
            ret = handle_delegate_addr(data, context);
            break;
        case TAG_CHAIN_ID:
            ret = handle_chain_id(data, context);
            break;
        case TAG_NONCE:
            ret = handle_nonce(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    return ret;
}

bool verify_auth_7702_struct(const s_auth_7702_ctx *context) {
    return ((context->mask_parsed & MASK_ALL) == MASK_ALL) && (context->version == STRUCT_VERSION);
}
