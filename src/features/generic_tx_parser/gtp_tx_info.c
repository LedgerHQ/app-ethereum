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
#include "tlv_library.h"
#include "tlv_apdu.h"

#define TX_INFO_TAGS(X)                                                            \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG)                       \
    X(0x01, TAG_CHAIN_ID, handle_chain_id, ENFORCE_UNIQUE_TAG)                     \
    X(0x02, TAG_CONTRACT_ADDR, handle_contract_addr, ENFORCE_UNIQUE_TAG)           \
    X(0x03, TAG_SELECTOR, handle_selector, ENFORCE_UNIQUE_TAG)                     \
    X(0x04, TAG_FIELDS_HASH, handle_fields_hash, ENFORCE_UNIQUE_TAG)               \
    X(0x05, TAG_OPERATION_TYPE, handle_operation_type, ENFORCE_UNIQUE_TAG)         \
    X(0x06, TAG_CREATOR_NAME, handle_creator_name, ALLOW_MULTIPLE_TAG)             \
    X(0x07, TAG_CREATOR_LEGAL_NAME, handle_creator_legal_name, ALLOW_MULTIPLE_TAG) \
    X(0x08, TAG_CREATOR_URL, handle_creator_url, ALLOW_MULTIPLE_TAG)               \
    X(0x09, TAG_CONTRACT_NAME, handle_contract_name, ALLOW_MULTIPLE_TAG)           \
    X(0x0a, TAG_DEPLOY_DATE, handle_deploy_date, ALLOW_MULTIPLE_TAG)               \
    X(0xff, TAG_SIGNATURE, handle_signature, ENFORCE_UNIQUE_TAG)

static bool handle_version(const tlv_data_t *data, s_tx_info_ctx *context) {
    return tlv_get_uint8(data, &context->tx_info->version, 0, UINT8_MAX);
}

static bool handle_chain_id(const tlv_data_t *data, s_tx_info_ctx *context) {
    return tlv_get_chain_id(data, &context->tx_info->chain_id);
}

static bool handle_contract_addr(const tlv_data_t *data, s_tx_info_ctx *context) {
    return tlv_get_address(data, context->tx_info->contract_addr, false);
}

static bool handle_selector(const tlv_data_t *data, s_tx_info_ctx *context) {
    uint8_t buf[CALLDATA_SELECTOR_SIZE];
    const uint8_t *selector;

    if (tlv_get_selector(data, buf, sizeof(buf)) != true) {
        return false;
    }
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
    return true;
}

static bool handle_fields_hash(const tlv_data_t *data, s_tx_info_ctx *context) {
    if (data->value.size > sizeof(context->tx_info->fields_hash)) {
        return false;
    }
    buf_shrink_expand(data->value.ptr,
                      data->value.size,
                      context->tx_info->fields_hash,
                      sizeof(context->tx_info->fields_hash));
    return true;
}

static bool handle_operation_type(const tlv_data_t *data, s_tx_info_ctx *context) {
    str_cpy_explicit_trunc((const char *) data->value.ptr,
                           data->value.size,
                           context->tx_info->operation_type,
                           sizeof(context->tx_info->operation_type));
    return true;
}

static bool handle_creator_name(const tlv_data_t *data, s_tx_info_ctx *context) {
    str_cpy_explicit_trunc((const char *) data->value.ptr,
                           data->value.size,
                           context->tx_info->creator_name,
                           sizeof(context->tx_info->creator_name));
    return true;
}

static bool handle_creator_legal_name(const tlv_data_t *data, s_tx_info_ctx *context) {
    str_cpy_explicit_trunc((const char *) data->value.ptr,
                           data->value.size,
                           context->tx_info->creator_legal_name,
                           sizeof(context->tx_info->creator_legal_name));
    return true;
}

static bool handle_creator_url(const tlv_data_t *data, s_tx_info_ctx *context) {
    str_cpy_explicit_trunc((const char *) data->value.ptr,
                           data->value.size,
                           context->tx_info->creator_url,
                           sizeof(context->tx_info->creator_url));
    return true;
}

static bool handle_contract_name(const tlv_data_t *data, s_tx_info_ctx *context) {
    str_cpy_explicit_trunc((const char *) data->value.ptr,
                           data->value.size,
                           context->tx_info->contract_name,
                           sizeof(context->tx_info->contract_name));
    return true;
}

static bool handle_deploy_date(const tlv_data_t *data, s_tx_info_ctx *context) {
    uint8_t buf[sizeof(uint32_t)] = {0};
    time_t timestamp;

    if (data->value.size > sizeof(buf)) {
        return false;
    }
    buf_shrink_expand(data->value.ptr, data->value.size, buf, sizeof(buf));
    timestamp = read_u32_be(buf, 0);
    if (!time_format_to_yyyymmdd(&timestamp,
                                 context->tx_info->deploy_date,
                                 sizeof(context->tx_info->deploy_date))) {
        return false;
    }
    return true;
}

static bool handle_signature(const tlv_data_t *data, s_tx_info_ctx *context) {
    if (data->value.size > sizeof(context->tx_info->signature)) {
        return false;
    }
    memcpy(context->tx_info->signature, data->value.ptr, data->value.size);
    context->tx_info->signature_len = data->value.size;
    return true;
}

static bool tx_info_common_handler(const tlv_data_t *data, s_tx_info_ctx *context);

DEFINE_TLV_PARSER(TX_INFO_TAGS, &tx_info_common_handler, tx_info_tlv_parser)

// Common handler to hash all tags except signature
static bool tx_info_common_handler(const tlv_data_t *data, s_tx_info_ctx *context) {
    if (data->tag != TAG_SIGNATURE) {
        hash_nbytes(data->raw.ptr, data->raw.size, (cx_hash_t *) &context->struct_hash);
    }
    return true;
}

bool handle_tx_info_struct(const buffer_t *buf, s_tx_info_ctx *context) {
    return tx_info_tlv_parser(buf, context, &context->received_tags);
}

bool verify_tx_info_struct(const s_tx_info_ctx *context) {
    uint8_t hash[INT256_LENGTH];

    // Check if struct version was provided
    if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_VERSION)) {
        PRINTF("Error: no struct version specified!\n");
        return false;
    }

    // Verify required fields based on version
    switch (context->tx_info->version) {
        case 1:
            if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags,
                                         TAG_VERSION,
                                         TAG_CHAIN_ID,
                                         TAG_CONTRACT_ADDR,
                                         TAG_SELECTOR,
                                         TAG_FIELDS_HASH,
                                         TAG_OPERATION_TYPE,
                                         TAG_SIGNATURE)) {
                PRINTF("Error: missing required field(s)\n");
                return false;
            }
            break;
        default:
            PRINTF("Error: unsupported tx info version (%u)\n", context->tx_info->version);
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
