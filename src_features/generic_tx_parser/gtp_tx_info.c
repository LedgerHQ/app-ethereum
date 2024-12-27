#ifdef HAVE_GENERIC_TX_PARSER

#include <time.h>
#include "gtp_tx_info.h"
#include "read.h"
#include "hash_bytes.h"
#include "network.h"         // get_tx_chain_id
#include "shared_context.h"  // txContext
#include "utils.h"
#include "time_format.h"
#include "calldata.h"
#include "public_keys.h"  // LEDGER_SIGNATURE_PUBLIC_KEY

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

s_tx_info *g_tx_info = NULL;
cx_sha3_t hash_ctx;

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
    if (chain_id != get_tx_chain_id()) {
        PRINTF("Error: chain ID mismatch!\n");
        return false;
    }
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
    if (memcmp(buf, txContext.content->destination, sizeof(buf)) != 0) {
        PRINTF("Error: contract address mismatch!\n");
        return false;
    }
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
    if ((selector = calldata_get_selector()) == NULL) {
        return false;
    }
    if (memcmp(selector, buf, sizeof(buf)) != 0) {
        PRINTF("Error: selector mismatch!\n");
        return false;
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
    if (check_signature_with_pubkey("TX info",
                                    hash,
                                    sizeof(hash),
                                    LEDGER_SIGNATURE_PUBLIC_KEY,
                                    sizeof(LEDGER_SIGNATURE_PUBLIC_KEY),
#ifdef HAVE_LEDGER_PKI
                                    CERTIFICATE_PUBLIC_KEY_USAGE_COIN_META,
#endif
                                    (uint8_t *) context->tx_info->signature,
                                    context->tx_info->signature_len) != CX_OK) {
        return false;
    }
    return true;
}

const char *get_operation_type(void) {
    if (g_tx_info->operation_type[0] == '\0') {
        return NULL;
    }
    return g_tx_info->operation_type;
}

const char *get_creator_name(void) {
    if (g_tx_info->creator_name[0] == '\0') {
        return NULL;
    }
    return g_tx_info->creator_name;
}

const char *get_creator_legal_name(void) {
    if (g_tx_info->creator_legal_name[0] == '\0') {
        return NULL;
    }
    return g_tx_info->creator_legal_name;
}

const char *get_creator_url(void) {
    if (g_tx_info->creator_url[0] == '\0') {
        return NULL;
    }
    return g_tx_info->creator_url;
}

const char *get_contract_name(void) {
    if (g_tx_info->contract_name[0] == '\0') {
        return NULL;
    }
    return g_tx_info->contract_name;
}

const uint8_t *get_contract_addr(void) {
    return g_tx_info->contract_addr;
}

const char *get_deploy_date(void) {
    if (g_tx_info->deploy_date[0] == '\0') {
        return NULL;
    }
    return g_tx_info->deploy_date;
}

cx_hash_t *get_fields_hash_ctx(void) {
    return (cx_hash_t *) &hash_ctx;
}

bool validate_instruction_hash(void) {
    uint8_t hash[sizeof(g_tx_info->fields_hash)];

    if (cx_hash_no_throw((cx_hash_t *) &hash_ctx, CX_LAST, NULL, 0, hash, sizeof(hash)) != CX_OK) {
        return false;
    }
    return memcmp(g_tx_info->fields_hash, hash, sizeof(hash)) == 0;
}

#endif  // HAVE_GENERIC_TX_PARSER
