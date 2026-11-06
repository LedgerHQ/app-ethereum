#include "map_entry.h"
#include "public_keys.h"
#include "utils.h"
#include "ui_utils.h"
#include "app_mem_utils.h"
#include "hash_bytes.h"
#include "tlv_utils.h"
#include "lcx_ecdsa.h"
#include "operation_ctx.h"

#define STRUCT_VERSION 0x01

// Cap on accepted map entries to bound the shared app-memory pool. Map entries
// are scoped to one transaction (cleared by reset_app_context), so this only
// needs to cover the largest legitimate per-tx use, not aggregate session use.
#define MAX_MAP_ENTRIES 32

static s_map_entry *g_map_entry_list = NULL;

static bool handle_version(const tlv_data_t *data, s_map_entry_ctx *context) {
    UNUSED(context);
    if (!tlv_enforce_u8_value(data, STRUCT_VERSION)) {
        PRINTF("Invalid STRUCTURE_VERSION value\n");
        return false;
    }
    return true;
}

static bool handle_chain_id(const tlv_data_t *data, s_map_entry_ctx *context) {
    return tlv_get_chain_id(data, &context->entry.operation_key.chain_id);
}

static bool handle_contract_addr(const tlv_data_t *data, s_map_entry_ctx *context) {
    return tlv_get_address(data, context->entry.operation_key.contract_addr);
}

// Which tells the entry is of a transaction
static bool handle_selector(const tlv_data_t *data, s_map_entry_ctx *context) {
    context->entry.operation_key.context = OPERATION_INFO_CTX_SIGN_TX;
    return tlv_get_hash(data,
                        context->entry.operation_key.selector,
                        sizeof(context->entry.operation_key.selector));
}

static bool handle_domain_separator(const tlv_data_t *data, s_map_entry_ctx *context) {
    return tlv_get_hash(data,
                        context->entry.operation_key.domain_separator,
                        sizeof(context->entry.operation_key.domain_separator));
}

// Which tells the entry is of a typed message
static bool handle_primary_type_hash(const tlv_data_t *data, s_map_entry_ctx *context) {
    context->entry.operation_key.context = OPERATION_INFO_CTX_EIP712;
    return tlv_get_hash(data,
                        context->entry.operation_key.primary_type_hash,
                        sizeof(context->entry.operation_key.primary_type_hash));
}

static bool handle_id(const tlv_data_t *data, s_map_entry_ctx *context) {
    return tlv_get_uint8_range(data, &context->entry.id, 0, UINT8_MAX);
}

static bool handle_key(const tlv_data_t *data, s_map_entry_ctx *context) {
    if (data->value.size == 0 || data->value.size > sizeof(context->entry.key)) {
        PRINTF("MAP_ENTRY KEY: invalid size %d\n", (int) data->value.size);
        return false;
    }
    context->entry.key_size = data->value.size;
    memcpy(context->entry.key, data->value.ptr, data->value.size);
    return true;
}

static bool handle_value(const tlv_data_t *data, s_map_entry_ctx *context) {
    if (data->value.size == 0 || data->value.size > sizeof(context->entry.value)) {
        PRINTF("MAP_ENTRY VALUE: invalid size %d\n", (int) data->value.size);
        return false;
    }
    context->entry.value_size = data->value.size;
    memcpy(context->entry.value, data->value.ptr, data->value.size);
    return true;
}

static bool handle_signature(const tlv_data_t *data, s_map_entry_ctx *context) {
    buffer_t sig = {0};
    if (!get_buffer_from_tlv_data(data,
                                  &sig,
                                  CX_ECDSA_SHA256_SIG_MIN_ASN1_LENGTH,
                                  CX_ECDSA_SHA256_SIG_MAX_ASN1_LENGTH)) {
        PRINTF("MAP_ENTRY SIGNATURE: failed to extract\n");
        return false;
    }
    context->sig_size = sig.size;
    context->sig = sig.ptr;
    return true;
}

#define MAP_ENTRY_TAGS(X)                                                        \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG)                     \
    X(0x01, TAG_CHAIN_ID, handle_chain_id, ENFORCE_UNIQUE_TAG)                   \
    X(0x02, TAG_CONTRACT_ADDR, handle_contract_addr, ENFORCE_UNIQUE_TAG)         \
    X(0x03, TAG_SELECTOR, handle_selector, ENFORCE_UNIQUE_TAG)                   \
    X(0x04, TAG_ID, handle_id, ENFORCE_UNIQUE_TAG)                               \
    X(0x05, TAG_KEY, handle_key, ENFORCE_UNIQUE_TAG)                             \
    X(0x06, TAG_VALUE, handle_value, ENFORCE_UNIQUE_TAG)                         \
    X(0x07, TAG_DOMAIN_SEPARATOR, handle_domain_separator, ENFORCE_UNIQUE_TAG)   \
    X(0x08, TAG_PRIMARY_TYPE_HASH, handle_primary_type_hash, ENFORCE_UNIQUE_TAG) \
    X(0xff, TAG_SIGNATURE, handle_signature, ENFORCE_UNIQUE_TAG)

static bool map_entry_common_handler(const tlv_data_t *data, s_map_entry_ctx *context);

DEFINE_TLV_PARSER(MAP_ENTRY_TAGS, &map_entry_common_handler, map_entry_tlv_parser)

static bool map_entry_common_handler(const tlv_data_t *data, s_map_entry_ctx *context) {
    if (data->tag != TAG_SIGNATURE) {
        hash_nbytes(data->raw.ptr, data->raw.size, (cx_hash_t *) &context->hash_ctx);
    }
    return true;
}

static bool verify_signature(const s_map_entry_ctx *context) {
    uint8_t hash[INT256_LENGTH];

    if (finalize_hash((cx_hash_t *) &context->hash_ctx, hash, sizeof(hash)) != true) {
        return false;
    }
    return check_signature_with_pubkey(hash,
                                       sizeof(hash),
                                       NULL,
                                       0,
                                       CERTIFICATE_PUBLIC_KEY_USAGE_CALLDATA,
                                       (uint8_t *) context->sig,
                                       context->sig_size);
}

static bool verify_fields(const s_map_entry_ctx *context) {
    TLV_reception_t tags = context->received_tags;
    bool of_tx = TLV_CHECK_RECEIVED_TAGS(tags, TAG_CHAIN_ID) ||
                 TLV_CHECK_RECEIVED_TAGS(tags, TAG_CONTRACT_ADDR) ||
                 TLV_CHECK_RECEIVED_TAGS(tags, TAG_SELECTOR);
    bool of_message = TLV_CHECK_RECEIVED_TAGS(tags, TAG_DOMAIN_SEPARATOR) ||
                      TLV_CHECK_RECEIVED_TAGS(tags, TAG_PRIMARY_TYPE_HASH);

    if (!TLV_CHECK_RECEIVED_TAGS(tags, TAG_VERSION, TAG_ID, TAG_KEY, TAG_VALUE, TAG_SIGNATURE)) {
        return false;
    }
    // the entry is attached to a function of a contract or to a primary type of a domain, whole,
    // never to both
    if (of_tx == of_message) {
        return false;
    }
    if (of_tx) {
        return TLV_CHECK_RECEIVED_TAGS(tags, TAG_CHAIN_ID, TAG_CONTRACT_ADDR, TAG_SELECTOR);
    }
    return TLV_CHECK_RECEIVED_TAGS(tags, TAG_DOMAIN_SEPARATOR, TAG_PRIMARY_TYPE_HASH);
}

bool handle_map_entry_tlv_payload(const buffer_t *buf, s_map_entry_ctx *context) {
    return map_entry_tlv_parser(buf, context, &context->received_tags);
}

bool verify_map_entry_struct(const s_map_entry_ctx *context) {
    s_map_entry *entry;

    if (!verify_fields(context)) {
        PRINTF("Error: Missing mandatory fields in MAP_ENTRY descriptor!\n");
        return false;
    }
    if (!verify_signature(context)) {
        PRINTF("Error: Signature verification failed for MAP_ENTRY descriptor!\n");
        return false;
    }
    if (flist_size((flist_node_t **) &g_map_entry_list) >= MAX_MAP_ENTRIES) {
        PRINTF("Error: MAP_ENTRY list cap reached (%d)\n", MAX_MAP_ENTRIES);
        return false;
    }
    if ((entry = APP_MEM_ALLOC(sizeof(*entry))) == NULL) {
        PRINTF("Error: Not enough memory for MAP_ENTRY!\n");
        return false;
    }
    memcpy(entry, &context->entry, sizeof(*entry));
    flist_push_back((flist_node_t **) &g_map_entry_list, (flist_node_t *) entry);
    return true;
}

const s_map_entry *get_matching_map_entry(uint8_t id, const uint8_t *key, uint8_t key_size) {
    s_operation_key operation_key;

    // the operation being described, which may be a call a message or a transaction makes
    if (!get_current_operation_key(&operation_key)) return NULL;

    for (const flist_node_t *tmp = (flist_node_t *) g_map_entry_list; tmp != NULL;
         tmp = tmp->next) {
        const s_map_entry *node = (const s_map_entry *) tmp;

        if (operation_key_matches(&node->operation_key, &operation_key) && (id == node->id) &&
            (key_size == node->key_size) && (memcmp(key, node->key, key_size) == 0)) {
            return node;
        }
    }
    return NULL;
}

static void delete_map_entry(s_map_entry *node) {
    APP_MEM_FREE(node);
}

void map_entry_cleanup(void) {
    flist_clear((flist_node_t **) &g_map_entry_list, (f_list_node_del) &delete_map_entry);
    g_map_entry_list = NULL;
}
