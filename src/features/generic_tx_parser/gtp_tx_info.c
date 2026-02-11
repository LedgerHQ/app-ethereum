#include <time.h>
#include "gtp_tx_info.h"
#include "read.h"
#include "hash_bytes.h"
#include "utils.h"
#include "time_format.h"
#include "calldata.h"
#include "public_keys.h"
#include "proxy_info.h"
#include "mem.h"
#include "tx_ctx.h"

enum {
    BIT_VERSION = 0,
    BIT_CHAIN_ID,
    BIT_CONTRACT_ADDR,
    BIT_SELECTOR,
    BIT_FIELDS_HASH,
    BIT_OPERATION_TYPE,
    BIT_CREATOR_NAME,
    BIT_CREATOR_LEGAL_NAME,
    BIT_CREATOR_URL,
    BIT_CONTRACT_NAME,
    BIT_DEPLOY_DATE,
    BIT_SIGNATURE,
};

enum {
    TAG_VERSION = 0x00,
    TAG_CHAIN_ID = 0x01,
    TAG_CONTRACT_ADDR = 0x02,
    TAG_SELECTOR = 0x03,
    TAG_FIELDS_HASH = 0x04,
    TAG_OPERATION_TYPE = 0x05,
    TAG_CREATOR_NAME = 0x06,
    TAG_CREATOR_LEGAL_NAME = 0x07,
    TAG_CREATOR_URL = 0x08,
    TAG_CONTRACT_NAME = 0x09,
    TAG_DEPLOY_DATE = 0x0a,
    TAG_SIGNATURE = 0xff,
};

static bool handle_version(const s_tlv_data *data, s_tx_info_ctx *context) {
    if (data->length != sizeof(context->tx_info->version)) {
        return false;
    }
    context->tx_info->version = data->value[0];
    context->set_flags |= SET_BIT(BIT_VERSION);
    return true;
}

static bool handle_chain_id(const s_tlv_data *data, s_tx_info_ctx *context) {
    uint64_t chain_id;
    uint8_t buf[sizeof(chain_id)] = {0};

    if (data->length > sizeof(buf)) {
        return false;
    }
    buf_shrink_expand(data->value, data->length, buf, sizeof(buf));
    chain_id = read_u64_be(buf, 0);
    context->tx_info->chain_id = chain_id;
    context->set_flags |= SET_BIT(BIT_CHAIN_ID);
    return true;
}

static bool handle_contract_addr(const s_tlv_data *data, s_tx_info_ctx *context) {
    uint8_t buf[ADDRESS_LENGTH] = {0};

    if (data->length > sizeof(buf)) {
        return false;
    }
    buf_shrink_expand(data->value, data->length, buf, sizeof(buf));
    memcpy(context->tx_info->contract_addr, buf, sizeof(buf));
    context->set_flags |= SET_BIT(BIT_CONTRACT_ADDR);
    return true;
}

static bool handle_selector(const s_tlv_data *data, s_tx_info_ctx *context) {
    uint8_t buf[CALLDATA_SELECTOR_SIZE];
    const uint8_t *selector;

    if (data->length > sizeof(buf)) {
        return false;
    }
    buf_shrink_expand(data->value, data->length, buf, sizeof(buf));
    if (get_tx_ctx_count() == 0) {
        if ((selector = calldata_get_selector(g_parked_calldata)) == NULL) {
            return false;
        }
        if (memcmp(selector, buf, sizeof(buf)) != 0) {
            PRINTF("Error: selector mismatch!\n");
            return false;
        }
    }
    memcpy(context->tx_info->selector, buf, sizeof(buf));
    context->set_flags |= SET_BIT(BIT_SELECTOR);
    return true;
}

static bool handle_fields_hash(const s_tlv_data *data, s_tx_info_ctx *context) {
    if (data->length > sizeof(context->tx_info->fields_hash)) {
        return false;
    }
    buf_shrink_expand(data->value,
                      data->length,
                      context->tx_info->fields_hash,
                      sizeof(context->tx_info->fields_hash));
    context->set_flags |= SET_BIT(BIT_FIELDS_HASH);
    return true;
}

static bool handle_operation_type(const s_tlv_data *data, s_tx_info_ctx *context) {
    str_cpy_explicit_trunc((const char *) data->value,
                           data->length,
                           context->tx_info->operation_type,
                           sizeof(context->tx_info->operation_type));
    context->set_flags |= SET_BIT(BIT_OPERATION_TYPE);
    return true;
}

static bool handle_creator_name(const s_tlv_data *data, s_tx_info_ctx *context) {
    str_cpy_explicit_trunc((const char *) data->value,
                           data->length,
                           context->tx_info->creator_name,
                           sizeof(context->tx_info->creator_name));
    context->set_flags |= SET_BIT(BIT_CREATOR_NAME);
    return true;
}

static bool handle_creator_legal_name(const s_tlv_data *data, s_tx_info_ctx *context) {
    str_cpy_explicit_trunc((const char *) data->value,
                           data->length,
                           context->tx_info->creator_legal_name,
                           sizeof(context->tx_info->creator_legal_name));
    context->set_flags |= SET_BIT(BIT_CREATOR_LEGAL_NAME);
    return true;
}

static bool handle_creator_url(const s_tlv_data *data, s_tx_info_ctx *context) {
    str_cpy_explicit_trunc((const char *) data->value,
                           data->length,
                           context->tx_info->creator_url,
                           sizeof(context->tx_info->creator_url));
    context->set_flags |= SET_BIT(BIT_CREATOR_URL);
    return true;
}

static bool handle_contract_name(const s_tlv_data *data, s_tx_info_ctx *context) {
    str_cpy_explicit_trunc((const char *) data->value,
                           data->length,
                           context->tx_info->contract_name,
                           sizeof(context->tx_info->contract_name));
    context->set_flags |= SET_BIT(BIT_CONTRACT_NAME);
    return true;
}

static bool handle_deploy_date(const s_tlv_data *data, s_tx_info_ctx *context) {
    uint8_t buf[sizeof(uint32_t)] = {0};
    time_t timestamp;

    if (data->length > sizeof(buf)) {
        return false;
    }
    buf_shrink_expand(data->value, data->length, buf, sizeof(buf));
    timestamp = read_u32_be(buf, 0);
    if (!time_format_to_yyyymmdd(&timestamp,
                                 context->tx_info->deploy_date,
                                 sizeof(context->tx_info->deploy_date))) {
        return false;
    }
    context->set_flags |= SET_BIT(BIT_DEPLOY_DATE);
    return true;
}

static bool handle_signature(const s_tlv_data *data, s_tx_info_ctx *context) {
    if (data->length > sizeof(context->tx_info->signature)) {
        return false;
    }
    memcpy(context->tx_info->signature, data->value, data->length);
    context->tx_info->signature_len = data->length;
    context->set_flags |= SET_BIT(BIT_SIGNATURE);
    return true;
}

bool handle_tx_info_struct(const s_tlv_data *data, s_tx_info_ctx *context) {
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
        case TAG_FIELDS_HASH:
            ret = handle_fields_hash(data, context);
            break;
        case TAG_OPERATION_TYPE:
            ret = handle_operation_type(data, context);
            break;
        case TAG_CREATOR_NAME:
            ret = handle_creator_name(data, context);
            break;
        case TAG_CREATOR_LEGAL_NAME:
            ret = handle_creator_legal_name(data, context);
            break;
        case TAG_CREATOR_URL:
            ret = handle_creator_url(data, context);
            break;
        case TAG_CONTRACT_NAME:
            ret = handle_contract_name(data, context);
            break;
        case TAG_DEPLOY_DATE:
            ret = handle_deploy_date(data, context);
            break;
        case TAG_SIGNATURE:
            ret = handle_signature(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    if (ret && (data->tag != TAG_SIGNATURE)) {
        hash_nbytes(data->raw, data->raw_size, (cx_hash_t *) &context->struct_hash);
    }
    return ret;
}

bool verify_tx_info_struct(const s_tx_info_ctx *context) {
    uint16_t required_bits = 0;
    uint8_t hash[INT256_LENGTH];

    // check if struct version was provided
    required_bits |= SET_BIT(BIT_VERSION);
    if ((context->set_flags & required_bits) != required_bits) {
        PRINTF("Error: no struct version specified!\n");
        return false;
    }

    // verify required fields
    switch (context->tx_info->version) {
        case 1:
            required_bits |=
                (SET_BIT(BIT_CHAIN_ID) | SET_BIT(BIT_CONTRACT_ADDR) | SET_BIT(BIT_SELECTOR) |
                 SET_BIT(BIT_FIELDS_HASH) | SET_BIT(BIT_OPERATION_TYPE) | SET_BIT(BIT_SIGNATURE));
            break;
        default:
            PRINTF("Error: unsupported tx info version (%u)\n", context->tx_info->version);
            return false;
    }

    if ((context->set_flags & required_bits) != required_bits) {
        PRINTF("Error: missing required field(s)\n");
        return false;
    }

    // verify signature
    if (finalize_hash((cx_hash_t *) &context->struct_hash, hash, sizeof(hash)) != true) {
        return false;
    }

    if (check_signature_with_pubkey(hash,
                                    sizeof(hash),
                                    CERTIFICATE_PUBLIC_KEY_USAGE_CALLDATA,
                                    (uint8_t *) context->tx_info->signature,
                                    context->tx_info->signature_len) != true) {
        return false;
    }
    return true;
}

const char *get_operation_type(const s_tx_info *tx_info) {
    if ((tx_info == NULL) || (tx_info->operation_type[0] == '\0')) {
        return NULL;
    }
    return tx_info->operation_type;
}

const char *get_creator_name(const s_tx_info *tx_info) {
    if ((tx_info == NULL) || (tx_info->creator_name[0] == '\0')) {
        return NULL;
    }
    return tx_info->creator_name;
}

const char *get_creator_legal_name(const s_tx_info *tx_info) {
    if ((tx_info == NULL) || (tx_info->creator_legal_name[0] == '\0')) {
        return NULL;
    }
    return tx_info->creator_legal_name;
}

const char *get_creator_url(const s_tx_info *tx_info) {
    if ((tx_info == NULL) || (tx_info->creator_url[0] == '\0')) {
        return NULL;
    }
    return tx_info->creator_url;
}

const char *get_contract_name(const s_tx_info *tx_info) {
    if ((tx_info == NULL) || (tx_info->contract_name[0] == '\0')) {
        return NULL;
    }
    return tx_info->contract_name;
}

const uint8_t *get_contract_addr(const s_tx_info *tx_info) {
    if (tx_info == NULL) {
        return NULL;
    }
    return tx_info->contract_addr;
}

const char *get_deploy_date(const s_tx_info *tx_info) {
    if ((tx_info == NULL) || (tx_info->deploy_date[0] == '\0')) {
        return NULL;
    }
    return tx_info->deploy_date;
}

void delete_tx_info(s_tx_info *node) {
    app_mem_free(node);
}
