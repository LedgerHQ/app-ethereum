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
#include "proxy_info.h"
#include "mem.h"

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

static s_tx_info *g_tx_info_list = NULL;
static s_tx_info *g_tx_info_current = NULL;

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
    if (get_tx_ctx_count() == 0) {
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
    const uint8_t *proxy_parent;
    uint64_t tx_chain_id;

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

    if (get_tx_ctx_count() == 0) {
        tx_chain_id = get_tx_chain_id();
        if (((proxy_parent = get_implem_contract(&tx_chain_id,
                                                 txContext.content->destination,
                                                 context->tx_info->selector)) == NULL) ||
            (memcmp(proxy_parent, context->tx_info->contract_addr, ADDRESS_LENGTH) != 0)) {
            if (memcmp(context->tx_info->contract_addr,
                       txContext.content->destination,
                       ADDRESS_LENGTH) != 0) {
                PRINTF("Error: contract address mismatch!\n");
                return false;
            }
        }
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

    if (check_signature_with_pubkey("TX info",
                                    hash,
                                    sizeof(hash),
                                    NULL,
                                    0,
                                    CERTIFICATE_PUBLIC_KEY_USAGE_CALLDATA,
                                    (uint8_t *) context->tx_info->signature,
                                    context->tx_info->signature_len) != CX_OK) {
        return false;
    }
    return true;
}

const char *get_operation_type(void) {
    if (g_tx_info_current->operation_type[0] == '\0') {
        return NULL;
    }
    return g_tx_info_current->operation_type;
}

const char *get_creator_name(void) {
    if (g_tx_info_current->creator_name[0] == '\0') {
        return NULL;
    }
    return g_tx_info_current->creator_name;
}

const char *get_creator_legal_name(void) {
    if (g_tx_info_current->creator_legal_name[0] == '\0') {
        return NULL;
    }
    return g_tx_info_current->creator_legal_name;
}

const char *get_creator_url(void) {
    if (g_tx_info_current->creator_url[0] == '\0') {
        return NULL;
    }
    return g_tx_info_current->creator_url;
}

const char *get_contract_name(void) {
    if (g_tx_info_current->contract_name[0] == '\0') {
        return NULL;
    }
    return g_tx_info_current->contract_name;
}

const uint8_t *get_contract_addr(void) {
    return g_tx_info_current->contract_addr;
}

const char *get_deploy_date(void) {
    if (g_tx_info_current->deploy_date[0] == '\0') {
        return NULL;
    }
    return g_tx_info_current->deploy_date;
}

cx_hash_t *get_fields_hash_ctx(void) {
    return (cx_hash_t *) &g_tx_info_current->fields_hash_ctx;
}

static bool validate_inst_hash_on(const s_tx_info *tx_info) {
    cx_sha3_t hash_ctx;
    uint8_t hash[sizeof(tx_info->fields_hash)];

    if (tx_info == NULL) return false;
    // copy it locally, because the cx_hash call will modify it
    memcpy(&hash_ctx, &tx_info->fields_hash_ctx, sizeof(hash_ctx));
    if (cx_hash_no_throw((cx_hash_t *) &hash_ctx,
                         CX_LAST,
                         NULL,
                         0,
                         hash,
                         sizeof(hash)) != CX_OK) {
        return false;
    }
    return memcmp(tx_info->fields_hash, hash, sizeof(hash)) == 0;
}

bool validate_instruction_hash(void) {
    return validate_inst_hash_on(g_tx_info_current);
}

void push_new_tx_ctx(s_tx_info *tx_info) {
    flist_push_back((s_flist_node **) &g_tx_info_list, (s_flist_node *) tx_info);
    g_tx_info_current = tx_info;
}

bool tx_ctx_is_root(void) {
    return g_tx_info_current == g_tx_info_list;
}

size_t get_tx_ctx_count(void) {
    return flist_size((s_flist_node **) &g_tx_info_list);
}

bool push_field_into_tx_ctx(const s_field *field) {
    s_field_list_node *node;

    if ((node = app_mem_alloc(sizeof(*node))) == NULL) {
        return false;
    }
    explicit_bzero(node, sizeof(*node));
    memcpy(&node->field, field, sizeof(*field));
    flist_push_back((s_flist_node **) &g_tx_info_current->fields, (s_flist_node *) node);
    return true;
}

s_tx_info *get_current_tx_ctx(void) {
    return g_tx_info_current;
}

static void delete_field(s_field_list_node *node) {
    app_mem_free(node);
}

static void delete_tx_info(s_tx_info *node) {
    if (node->fields != NULL) flist_clear((s_flist_node **) &node->fields, (f_list_node_del) &delete_field);
    app_mem_free(node);
}

// TODO: make doubly linked
// g_tx_info_current = g_tx_info_current->prev;
void tx_info_move_to_parent(void) {
    s_flist_node *tmp = (s_flist_node *) g_tx_info_list;

    while ((tmp != NULL) && (tmp->next != (s_flist_node *) g_tx_info_current)) {
        tmp = tmp->next;
    }
    g_tx_info_current = (s_tx_info *) tmp;
}

void tx_info_pop(void) {
    flist_pop_back((s_flist_node **) &g_tx_info_list, (f_list_node_del) &delete_tx_info);
}

void tx_info_cleanup(void) {
    flist_clear((s_flist_node **) &g_tx_info_list, (f_list_node_del) &delete_tx_info);
}

bool find_matching_tx_info(const uint8_t *contract_addr, const uint8_t *selector, const uint64_t *chain_id) {
    for (s_tx_info *tmp = g_tx_info_list;
         tmp != NULL;
         tmp = (s_tx_info *) ((s_flist_node *) tmp)->next) {
        if ((memcmp(contract_addr, tmp->contract_addr, ADDRESS_LENGTH) == 0) &&
            (memcmp(selector, tmp->selector, CALLDATA_SELECTOR_SIZE) == 0) &&
            (*chain_id == tmp->chain_id)) {
            g_tx_info_current = tmp;
            return true;
        }
    }
    return false;
}

s_field_list_node *get_fields_list(void) {
    return g_tx_info_current->fields;
}
