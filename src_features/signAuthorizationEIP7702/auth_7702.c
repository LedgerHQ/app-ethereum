#ifdef HAVE_EIP7702

#include "auth_7702.h"
#include "utils.h"
#include "read.h"

enum {
    TAG_STRUCT_VERSION = 0x00,
    TAG_DERIVATION_IDX = 0x01,
    TAG_DELEGATE_ADDR = 0x02,
    TAG_CHAIN_ID = 0x03,
    TAG_NONCE = 0x04,
};

enum {
    BIT_VERSION,
    BIT_DERIVATION_IDX,
    BIT_DELEGATE_ADDR,
    BIT_CHAIN_ID,
    BIT_NONCE,
};

#define STRUCT_VERSION 0x01
#define MASK_ALL                                                                       \
    (SET_BIT(BIT_VERSION) | SET_BIT(BIT_DERIVATION_IDX) | SET_BIT(BIT_DELEGATE_ADDR) | \
     SET_BIT(BIT_CHAIN_ID) | SET_BIT(BIT_NONCE))

static bool handle_version(const s_tlv_data *data, s_auth_7702_ctx *context) {
    if (data->length != sizeof(uint8_t)) {
        return false;
    }
    context->mask_parsed |= SET_BIT(BIT_VERSION);
    return (data->value[0] == STRUCT_VERSION);
}

static bool handle_derivation_idx(const s_tlv_data *data, s_auth_7702_ctx *context) {
    uint32_t idx;
    if (data->length != sizeof(uint32_t)) {
        return false;
    }
    if (context->auth_7702.bip32.length == MAX_BIP32_PATH) {
        return false;
    }
    idx = read_u32_be(data->value, 0);
    context->auth_7702.bip32.path[context->auth_7702.bip32.length] = idx;
    context->auth_7702.bip32.length++;
    context->mask_parsed |= SET_BIT(BIT_DERIVATION_IDX);
    return true;
}

static bool handle_delegate_addr(const s_tlv_data *data, s_auth_7702_ctx *context) {
    if (data->length != sizeof(context->auth_7702.delegate)) {
        return false;
    }
    memmove(context->auth_7702.delegate, data->value, sizeof(context->auth_7702.delegate));
    context->mask_parsed |= SET_BIT(BIT_DELEGATE_ADDR);
    return true;
}

static bool handle_chain_id(const s_tlv_data *data, s_auth_7702_ctx *context) {
    if (data->length != sizeof(uint64_t)) {
        return false;
    }
    context->auth_7702.chainId = read_u64_be(data->value, 0);
    context->mask_parsed |= SET_BIT(BIT_CHAIN_ID);
    return true;
}

static bool handle_nonce(const s_tlv_data *data, s_auth_7702_ctx *context) {
    if (data->length != sizeof(uint64_t)) {
        return false;
    }
    context->auth_7702.nonce = read_u64_be(data->value, 0);
    context->mask_parsed |= SET_BIT(BIT_NONCE);
    return true;
}

bool handle_auth_7702_struct(const s_tlv_data *data, s_auth_7702_ctx *context) {
    bool ret;

    switch (data->tag) {
        case TAG_STRUCT_VERSION:
            ret = handle_version(data, context);
            break;
        case TAG_DERIVATION_IDX:
            ret = handle_derivation_idx(data, context);
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

bool verify_auth_7702_struct(s_auth_7702_ctx *context) {
    return ((context->mask_parsed & MASK_ALL) == MASK_ALL);
}

#endif  // HAVE_EIP7702