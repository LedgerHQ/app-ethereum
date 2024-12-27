#ifdef HAVE_ENUM_VALUE

#include <string.h>
#include "enum_value.h"
#include "read.h"
#include "public_keys.h"
#include "utils.h"

enum {
    TAG_VERSION = 0x00,
    TAG_CHAIN_ID = 0x01,
    TAG_CONTRACT_ADDR = 0x02,
    TAG_SELECTOR = 0x03,
    TAG_ID = 0x04,
    TAG_VALUE = 0x05,
    TAG_NAME = 0x06,
    TAG_SIGNATURE = 0xff,
};

static s_enum_value_entry g_enum_value = {0};

static bool handle_version(const s_tlv_data *data, s_enum_value_ctx *context) {
    if (data->length != sizeof(context->enum_value.version)) {
        return false;
    }
    context->enum_value.version = data->value[0];
    return true;
}

static bool handle_chain_id(const s_tlv_data *data, s_enum_value_ctx *context) {
    uint8_t buf[sizeof(context->enum_value.entry.chain_id)] = {0};

    if (data->length > sizeof(buf)) {
        return false;
    }
    buf_shrink_expand(data->value, data->length, buf, sizeof(buf));
    context->enum_value.entry.chain_id = read_u64_be(buf, 0);
    return true;
}

static bool handle_contract_addr(const s_tlv_data *data, s_enum_value_ctx *context) {
    if (data->length > sizeof(context->enum_value.entry.contract_addr)) {
        return false;
    }
    buf_shrink_expand(data->value,
                      data->length,
                      context->enum_value.entry.contract_addr,
                      sizeof(context->enum_value.entry.contract_addr));
    return true;
}

static bool handle_selector(const s_tlv_data *data, s_enum_value_ctx *context) {
    if (data->length > sizeof(context->enum_value.entry.selector)) {
        return false;
    }
    buf_shrink_expand(data->value,
                      data->length,
                      context->enum_value.entry.selector,
                      sizeof(context->enum_value.entry.selector));
    return true;
}

static bool handle_id(const s_tlv_data *data, s_enum_value_ctx *context) {
    if (data->length != sizeof(context->enum_value.entry.id)) {
        return false;
    }
    context->enum_value.entry.id = data->value[0];
    return true;
}

static bool handle_value(const s_tlv_data *data, s_enum_value_ctx *context) {
    if (data->length != sizeof(context->enum_value.entry.value)) {
        return false;
    }
    context->enum_value.entry.value = data->value[0];
    return true;
}

static bool handle_name(const s_tlv_data *data, s_enum_value_ctx *context) {
    str_cpy_explicit_trunc((const char *) data->value,
                           data->length,
                           context->enum_value.entry.name,
                           sizeof(context->enum_value.entry.name));
    return true;
}

static bool handle_signature(const s_tlv_data *data, s_enum_value_ctx *context) {
    if (data->length > sizeof(context->enum_value.signature)) {
        return false;
    }
    context->enum_value.signature_length = data->length;
    memcpy(context->enum_value.signature, data->value, data->length);
    return true;
}

bool handle_enum_value_struct(const s_tlv_data *data, s_enum_value_ctx *context) {
    bool ret;

    switch (data->tag) {
        case TAG_VERSION:
            ret = handle_version(data, context);
            break;
        case TAG_CHAIN_ID:
            ret = handle_chain_id(data, context);
            break;
        case TAG_CONTRACT_ADDR:
            ret = handle_contract_addr(data, context);
            break;
        case TAG_SELECTOR:
            ret = handle_selector(data, context);
            break;
        case TAG_ID:
            ret = handle_id(data, context);
            break;
        case TAG_VALUE:
            ret = handle_value(data, context);
            break;
        case TAG_NAME:
            ret = handle_name(data, context);
            break;
        case TAG_SIGNATURE:
            ret = handle_signature(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    if (ret && (data->tag != TAG_SIGNATURE)) {
        if (cx_hash_no_throw((cx_hash_t *) &context->struct_hash,
                             0,
                             data->raw,
                             data->raw_size,
                             NULL,
                             0) != CX_OK) {
            return false;
        }
    }
    return ret;
}

bool verify_enum_value_struct(const s_enum_value_ctx *context) {
    uint8_t hash[INT256_LENGTH];

    if (cx_hash_no_throw((cx_hash_t *) &context->struct_hash,
                         CX_LAST,
                         NULL,
                         0,
                         hash,
                         sizeof(hash)) != CX_OK) {
        PRINTF("Could not finalize struct hash!\n");
        return false;
    }
    // TODO: change to LEDGER_CALLDATA_DESCRIPTOR key once available
    if (check_signature_with_pubkey("enum value",
                                    hash,
                                    sizeof(hash),
                                    LEDGER_SIGNATURE_PUBLIC_KEY,
                                    sizeof(LEDGER_SIGNATURE_PUBLIC_KEY),
#ifdef HAVE_LEDGER_PKI
                                    CERTIFICATE_PUBLIC_KEY_USAGE_COIN_META,
#endif
                                    (uint8_t *) context->enum_value.signature,
                                    context->enum_value.signature_length) != CX_OK) {
        return false;
    }
    memcpy(&g_enum_value, &context->enum_value.entry, sizeof(g_enum_value));
    return true;
}

const char *get_matching_enum_name(const uint64_t *chain_id,
                                   const uint8_t *contract_addr,
                                   const uint8_t *selector,
                                   uint8_t id,
                                   uint8_t value) {
    if ((*chain_id == g_enum_value.chain_id) &&
        (memcmp(contract_addr, g_enum_value.contract_addr, ADDRESS_LENGTH) == 0) &&
        (memcmp(selector, g_enum_value.selector, SELECTOR_SIZE) == 0) && (id == g_enum_value.id) &&
        (value == g_enum_value.value)) {
        return g_enum_value.name;
    }
    return NULL;
}

#endif  // HAVE_ENUM_VALUE
