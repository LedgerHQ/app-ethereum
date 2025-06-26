#include "proxy_info.h"
#include "read.h"
#include "utils.h"  // buf_shrink_expand
#include "challenge.h"
#include "public_keys.h"
#include "ui_utils.h"
#include "mem_utils.h"

enum {
    TAG_STRUCT_TYPE = 0x01,
    TAG_STRUCT_VERSION = 0x02,
    TAG_CHALLENGE = 0x012,
    TAG_ADDRESS = 0x22,
    TAG_CHAIN_ID = 0x23,
    TAG_SELECTOR = 0x41,
    TAG_IMPLEM_ADDRESS = 0x42,
    TAG_DELEGATION_TYPE = 0x43,
    TAG_SIGNATURE = 0x15,
};

#define TYPE_PROXY_INFO 0x26

static s_proxy_info *g_proxy_info = NULL;

static bool handle_type(const s_tlv_data *data, s_proxy_info_ctx *context) {
    (void) context;

    if (data->length != sizeof(context->struct_type)) {
        return false;
    }
    context->struct_type = data->value[0];
    return true;
}

static bool handle_version(const s_tlv_data *data, s_proxy_info_ctx *context) {
    if (data->length != sizeof(context->version)) {
        return false;
    }
    context->version = data->value[0];
    return true;
}

static bool handle_challenge(const s_tlv_data *data, s_proxy_info_ctx *context) {
    uint8_t buf[sizeof(context->challenge)];

    if (data->length > sizeof(buf)) {
        return false;
    }
    buf_shrink_expand(data->value, data->length, buf, sizeof(buf));
    context->challenge = read_u32_be(buf, 0);
    return true;
}

static bool handle_address(const s_tlv_data *data, s_proxy_info_ctx *context) {
    if (data->length > sizeof(context->proxy_info.address)) {
        return false;
    }
    buf_shrink_expand(data->value,
                      data->length,
                      context->proxy_info.address,
                      sizeof(context->proxy_info.address));
    return true;
}

static bool handle_chain_id(const s_tlv_data *data, s_proxy_info_ctx *context) {
    uint8_t buf[sizeof(context->proxy_info.chain_id)];

    if (data->length > sizeof(buf)) {
        return false;
    }
    buf_shrink_expand(data->value, data->length, buf, sizeof(buf));
    context->proxy_info.chain_id = read_u64_be(buf, 0);
    return true;
}

static bool handle_selector(const s_tlv_data *data, s_proxy_info_ctx *context) {
    if (data->length > sizeof(context->proxy_info.selector)) {
        return false;
    }
    buf_shrink_expand(data->value,
                      data->length,
                      context->proxy_info.selector,
                      sizeof(context->proxy_info.selector));
    context->proxy_info.has_selector = true;
    return true;
}

static bool handle_implem_address(const s_tlv_data *data, s_proxy_info_ctx *context) {
    if (data->length > sizeof(context->proxy_info.implem_address)) {
        return false;
    }
    buf_shrink_expand(data->value,
                      data->length,
                      context->proxy_info.implem_address,
                      sizeof(context->proxy_info.implem_address));
    return true;
}

static bool handle_delegation_type(const s_tlv_data *data, s_proxy_info_ctx *context) {
    if (data->length != sizeof(context->delegation_type)) {
        return false;
    }
    context->delegation_type = data->value[0];
    return true;
}

static bool handle_signature(const s_tlv_data *data, s_proxy_info_ctx *context) {
    if (data->length > sizeof(context->signature)) {
        return false;
    }
    context->signature_length = data->length;
    memcpy(context->signature, data->value, data->length);
    return true;
}

bool handle_proxy_info_struct(const s_tlv_data *data, s_proxy_info_ctx *context) {
    bool ret;

    switch (data->tag) {
        case TAG_STRUCT_TYPE:
            ret = handle_type(data, context);
            break;
        case TAG_STRUCT_VERSION:
            ret = handle_version(data, context);
            break;
        case TAG_CHALLENGE:
            ret = handle_challenge(data, context);
            break;
        case TAG_ADDRESS:
            ret = handle_address(data, context);
            break;
        case TAG_CHAIN_ID:
            ret = handle_chain_id(data, context);
            break;
        case TAG_SELECTOR:
            ret = handle_selector(data, context);
            break;
        case TAG_IMPLEM_ADDRESS:
            ret = handle_implem_address(data, context);
            break;
        case TAG_DELEGATION_TYPE:
            ret = handle_delegation_type(data, context);
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

bool verify_proxy_info_struct(const s_proxy_info_ctx *context) {
    uint8_t hash[INT256_LENGTH];
    uint32_t challenge;

    if (cx_hash_no_throw((cx_hash_t *) &context->struct_hash,
                         CX_LAST,
                         NULL,
                         0,
                         hash,
                         sizeof(hash)) != CX_OK) {
        PRINTF("Error: could not finalize struct hash!\n");
        return false;
    }
    if (context->struct_type != TYPE_PROXY_INFO) {
        PRINTF("Error: unknown struct type (%u)!\n", context->struct_type);
        return false;
    }
    challenge = get_challenge();
    roll_challenge();
    if (context->challenge != challenge) {
        PRINTF("Error: challenge mismatch!\n");
        return false;
    }
    if (context->delegation_type != DELEGATION_TYPE_PROXY) {
        PRINTF("Error: unsupported delegation type (%u)!\n", context->delegation_type);
        return false;
    }
    if (check_signature_with_pubkey("proxy info",
                                    hash,
                                    sizeof(hash),
                                    NULL,
                                    0,
                                    CERTIFICATE_PUBLIC_KEY_USAGE_CALLDATA,
                                    (uint8_t *) context->signature,
                                    context->signature_length) != CX_OK) {
        PRINTF("Error: signature verification failed!\n");
        return false;
    }
    if (mem_buffer_allocate((void **) &g_proxy_info, sizeof(s_proxy_info)) == false) {
        PRINTF("Error: Not enough memory!\n");
        return false;
    }
    memcpy(g_proxy_info, &context->proxy_info, sizeof(s_proxy_info));
    PRINTF("================== PROXY INFO ====================\n");
    PRINTF("chain ID = %u\n", (uint32_t) g_proxy_info->chain_id);
    PRINTF("address = 0x%.*h\n", sizeof(g_proxy_info->address), g_proxy_info->address);
    PRINTF("implementation address = 0x%.*h\n",
           sizeof(g_proxy_info->implem_address),
           g_proxy_info->implem_address);
    if (g_proxy_info->has_selector) {
        PRINTF("selector = 0x%.*h\n", sizeof(g_proxy_info->selector), g_proxy_info->selector);
    }
    PRINTF("==================================================\n");
    return true;
}

static bool check_proxy_params(const uint64_t *chain_id,
                               const uint8_t *addr,
                               const uint8_t *selector,
                               const uint64_t *ref_chain_id,
                               const uint8_t *ref_addr,
                               const uint8_t *ref_selector) {
    if ((chain_id == NULL) || (*chain_id != *ref_chain_id)) {
        return false;
    }
    if (memcmp(addr, ref_addr, ADDRESS_LENGTH) != 0) {
        return false;
    }
    if (selector != NULL) {
        if (memcmp(selector, ref_selector, CALLDATA_SELECTOR_SIZE) != 0) {
            return false;
        }
    }
    return true;
}

const uint8_t *get_proxy_contract(const uint64_t *chain_id,
                                  const uint8_t *addr,
                                  const uint8_t *selector) {
    if (g_proxy_info == NULL) {
        PRINTF("Error: Proxy info not initialized!\n");
        return NULL;
    }
    if (!check_proxy_params(chain_id,
                            addr,
                            selector,
                            &g_proxy_info->chain_id,
                            g_proxy_info->implem_address,
                            g_proxy_info->selector)) {
        return NULL;
    }
    return g_proxy_info->address;
}

const uint8_t *get_implem_contract(const uint64_t *chain_id,
                                   const uint8_t *addr,
                                   const uint8_t *selector) {
    if (g_proxy_info == NULL) {
        PRINTF("Error: Proxy info not initialized!\n");
        return NULL;
    }
    if (!check_proxy_params(chain_id,
                            addr,
                            selector,
                            &g_proxy_info->chain_id,
                            g_proxy_info->address,
                            g_proxy_info->selector)) {
        return NULL;
    }
    return g_proxy_info->implem_address;
}

void proxy_cleanup(void) {
    mem_buffer_cleanup((void **) &g_proxy_info);
}
