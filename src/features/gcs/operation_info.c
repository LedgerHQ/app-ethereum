#include <time.h>
#include "operation_info.h"
#include "read.h"
#include "hash_bytes.h"
#include "utils.h"
#include "time_format.h"
#include "public_keys.h"  // LEDGER_SIGNATURE_PUBLIC_KEY
#include "app_mem_utils.h"
#include "tlv_library.h"
#include "tlv_utils.h"

typedef struct {
    cx_sha256_t struct_hash;
    s_operation_info *info;
    TLV_reception_t received_tags;
} s_operation_info_ctx;

#define OPERATION_INFO_TAGS(X)                                                     \
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
    X(0x0b, TAG_DOMAIN_SEPARATOR, handle_domain_separator, ENFORCE_UNIQUE_TAG)     \
    X(0x0c, TAG_PRIMARY_TYPE_HASH, handle_primary_type_hash, ENFORCE_UNIQUE_TAG)   \
    X(0xff, TAG_SIGNATURE, handle_signature, ENFORCE_UNIQUE_TAG)

static bool handle_version(const tlv_data_t *data, s_operation_info_ctx *context) {
    return tlv_get_uint8_range(data, &context->info->version, 0, UINT8_MAX);
}

// The chain, the contract and the called function identify a transaction, the domain and the
// primary type a typed message: each is refused in the other context
static bool handle_chain_id(const tlv_data_t *data, s_operation_info_ctx *context) {
    if (context->info->context != OPERATION_INFO_CTX_SIGN_TX) {
        return false;
    }
    return tlv_get_chain_id(data, &context->info->chain_id);
}

static bool handle_contract_addr(const tlv_data_t *data, s_operation_info_ctx *context) {
    if (context->info->context != OPERATION_INFO_CTX_SIGN_TX) {
        return false;
    }
    return tlv_get_address(data, context->info->contract_addr);
}

static bool handle_selector(const tlv_data_t *data, s_operation_info_ctx *context) {
    if (context->info->context != OPERATION_INFO_CTX_SIGN_TX) {
        return false;
    }
    return tlv_get_hash(data, context->info->selector, sizeof(context->info->selector));
}

static bool handle_domain_separator(const tlv_data_t *data, s_operation_info_ctx *context) {
    if (context->info->context != OPERATION_INFO_CTX_EIP712) {
        return false;
    }
    return tlv_get_hash(data,
                        context->info->domain_separator,
                        sizeof(context->info->domain_separator));
}

static bool handle_primary_type_hash(const tlv_data_t *data, s_operation_info_ctx *context) {
    if (context->info->context != OPERATION_INFO_CTX_EIP712) {
        return false;
    }
    return tlv_get_hash(data,
                        context->info->primary_type_hash,
                        sizeof(context->info->primary_type_hash));
}

static bool handle_fields_hash(const tlv_data_t *data, s_operation_info_ctx *context) {
    return tlv_get_hash(data, context->info->fields_hash, sizeof(context->info->fields_hash));
}

static bool handle_operation_type(const tlv_data_t *data, s_operation_info_ctx *context) {
    str_cpy_explicit_trunc((const char *) data->value.ptr,
                           data->value.size,
                           context->info->operation_type,
                           sizeof(context->info->operation_type));
    return true;
}

static bool handle_creator_name(const tlv_data_t *data, s_operation_info_ctx *context) {
    str_cpy_explicit_trunc((const char *) data->value.ptr,
                           data->value.size,
                           context->info->creator_name,
                           sizeof(context->info->creator_name));
    return true;
}

static bool handle_creator_legal_name(const tlv_data_t *data, s_operation_info_ctx *context) {
    str_cpy_explicit_trunc((const char *) data->value.ptr,
                           data->value.size,
                           context->info->creator_legal_name,
                           sizeof(context->info->creator_legal_name));
    return true;
}

static bool handle_creator_url(const tlv_data_t *data, s_operation_info_ctx *context) {
    str_cpy_explicit_trunc((const char *) data->value.ptr,
                           data->value.size,
                           context->info->creator_url,
                           sizeof(context->info->creator_url));
    return true;
}

static bool handle_contract_name(const tlv_data_t *data, s_operation_info_ctx *context) {
    str_cpy_explicit_trunc((const char *) data->value.ptr,
                           data->value.size,
                           context->info->contract_name,
                           sizeof(context->info->contract_name));
    return true;
}

static bool handle_deploy_date(const tlv_data_t *data, s_operation_info_ctx *context) {
    uint8_t buf[sizeof(uint32_t)] = {0};
    time_t timestamp;

    if (data->value.size > sizeof(buf)) {
        return false;
    }
    buf_shrink_expand(data->value.ptr, data->value.size, buf, sizeof(buf));
    timestamp = read_u32_be(buf, 0);
    if (!time_format_to_yyyymmdd(&timestamp,
                                 context->info->deploy_date,
                                 sizeof(context->info->deploy_date))) {
        return false;
    }
    return true;
}

static bool handle_signature(const tlv_data_t *data, s_operation_info_ctx *context) {
    if (data->value.size > sizeof(context->info->signature)) {
        return false;
    }
    if (data->value.size > 0) {
        if (data->value.ptr == NULL) {
            return false;
        }
        memcpy(context->info->signature, data->value.ptr, data->value.size);
    }
    context->info->signature_len = data->value.size;
    return true;
}

static bool operation_info_common_handler(const tlv_data_t *data, s_operation_info_ctx *context);

DEFINE_TLV_PARSER(OPERATION_INFO_TAGS, &operation_info_common_handler, operation_info_tlv_parser)

// Common handler to hash all tags except signature
static bool operation_info_common_handler(const tlv_data_t *data, s_operation_info_ctx *context) {
    if (data->tag != TAG_SIGNATURE) {
        hash_nbytes(data->raw.ptr, data->raw.size, (cx_hash_t *) &context->struct_hash);
    }
    return true;
}

static bool handle_operation_info_struct(const buffer_t *buf, s_operation_info_ctx *context) {
    return operation_info_tlv_parser(buf, context, &context->received_tags);
}

// What identifies the operation, in the context it was received in, and the tags both share
static bool has_required_tags(const s_operation_info_ctx *context) {
    switch (context->info->context) {
        case OPERATION_INFO_CTX_SIGN_TX:
            return TLV_CHECK_RECEIVED_TAGS(context->received_tags,
                                           TAG_VERSION,
                                           TAG_CHAIN_ID,
                                           TAG_CONTRACT_ADDR,
                                           TAG_SELECTOR,
                                           TAG_FIELDS_HASH,
                                           TAG_OPERATION_TYPE,
                                           TAG_SIGNATURE);
        case OPERATION_INFO_CTX_EIP712:
            return TLV_CHECK_RECEIVED_TAGS(context->received_tags,
                                           TAG_VERSION,
                                           TAG_DOMAIN_SEPARATOR,
                                           TAG_PRIMARY_TYPE_HASH,
                                           TAG_FIELDS_HASH,
                                           TAG_OPERATION_TYPE,
                                           TAG_SIGNATURE);
        default:
            return false;
    }
}

static bool verify_operation_info_struct(const s_operation_info_ctx *context) {
    uint8_t hash[INT256_LENGTH];

    // Check if struct version was provided
    if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_VERSION)) {
        PRINTF("Error: no struct version specified!\n");
        return false;
    }

    // Verify required fields based on version
    switch (context->info->version) {
        case 1:
            if (!has_required_tags(context)) {
                PRINTF("Error: missing required field(s)\n");
                return false;
            }
            break;
        default:
            PRINTF("Error: unsupported tx info version (%u)\n", context->info->version);
            return false;
    }

    // verify signature
    if (finalize_hash((cx_hash_t *) &context->struct_hash, hash, sizeof(hash)) != true) {
        return false;
    }

    if (check_signature_with_pubkey(hash,
                                    sizeof(hash),
                                    NULL,
                                    0,
                                    CERTIFICATE_PUBLIC_KEY_USAGE_CALLDATA,
                                    (uint8_t *) context->info->signature,
                                    context->info->signature_len) != true) {
        return false;
    }
    return true;
}

const char *get_operation_type(const s_operation_info *info) {
    if ((info == NULL) || (info->operation_type[0] == '\0')) {
        return NULL;
    }
    return info->operation_type;
}

const char *get_creator_name(const s_operation_info *info) {
    if ((info == NULL) || (info->creator_name[0] == '\0')) {
        return NULL;
    }
    return info->creator_name;
}

const char *get_creator_legal_name(const s_operation_info *info) {
    if ((info == NULL) || (info->creator_legal_name[0] == '\0')) {
        return NULL;
    }
    return info->creator_legal_name;
}

const char *get_creator_url(const s_operation_info *info) {
    if ((info == NULL) || (info->creator_url[0] == '\0')) {
        return NULL;
    }
    return info->creator_url;
}

const char *get_contract_name(const s_operation_info *info) {
    if ((info == NULL) || (info->contract_name[0] == '\0')) {
        return NULL;
    }
    return info->contract_name;
}

const uint8_t *get_selector(const s_operation_info *info) {
    if ((info == NULL) || (info->context != OPERATION_INFO_CTX_SIGN_TX)) {
        return NULL;
    }
    return info->selector;
}

const uint8_t *get_primary_type_hash(const s_operation_info *info) {
    if ((info == NULL) || (info->context != OPERATION_INFO_CTX_EIP712)) {
        return NULL;
    }
    return info->primary_type_hash;
}

const uint8_t *get_domain_separator(const s_operation_info *info) {
    if ((info == NULL) || (info->context != OPERATION_INFO_CTX_EIP712)) {
        return NULL;
    }
    return info->domain_separator;
}

const char *get_deploy_date(const s_operation_info *info) {
    if ((info == NULL) || (info->deploy_date[0] == '\0')) {
        return NULL;
    }
    return info->deploy_date;
}

s_operation_info *new_operation_info(const buffer_t *buf, e_operation_info_context context) {
    s_operation_info_ctx ctx = {0};

    if (APP_MEM_CALLOC((void **) &ctx.info, sizeof(*ctx.info)) == false) {
        return NULL;
    }
    ctx.info->context = context;
    cx_sha256_init(&ctx.struct_hash);
    if (!handle_operation_info_struct(buf, &ctx) || !verify_operation_info_struct(&ctx)) {
        delete_operation_info(ctx.info);
        return NULL;
    }
    return ctx.info;
}

bool operation_info_fields_hash_is_empty(const s_operation_info *info, bool *is_empty) {
    cx_sha3_t ctx;
    uint8_t hash[INT256_LENGTH];

    if ((info == NULL) || (is_empty == NULL)) {
        return false;
    }
    if (cx_sha3_init_no_throw(&ctx, 256) != CX_OK) {
        return false;
    }
    if (finalize_hash((cx_hash_t *) &ctx, hash, sizeof(hash)) != true) {
        return false;
    }
    *is_empty = (memcmp(hash, info->fields_hash, sizeof(hash)) == 0);
    return true;
}

void delete_operation_info(s_operation_info *node) {
    APP_MEM_FREE(node);
}
