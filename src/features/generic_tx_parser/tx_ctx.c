#include "tx_ctx.h"
#include "app_mem_utils.h"
#include "address_name_lookup.h"
#include "gtp_field_table.h"
#include "proxy_info.h"
#include "common_ui.h"       // ui_gcs_cleanup
#include "shared_context.h"  // appState
#include "utils.h"           // buf_shrink_expand
#include "get_public_key.h"
#include "eip712_v1_context.h"
#include "network.h"
#include "hash_bytes.h"

static s_tx_ctx *g_tx_ctx_list = NULL;
static s_tx_ctx *g_tx_ctx_current = NULL;
s_calldata *g_parked_calldata = NULL;
// Whether a descriptor field already displays the root transaction native value (CP_VALUE)
static bool g_root_value_shown = false;
// Whether a descriptor field already displays the root transaction sender (CP_FROM)
static bool g_root_from_shown = false;

bool tx_ctx_is_root(void) {
    return (g_tx_ctx_list != NULL) && (g_tx_ctx_current == g_tx_ctx_list);
}

size_t get_tx_ctx_count(void) {
    return list_size((list_node_t **) &g_tx_ctx_list);
}

cx_hash_t *get_fields_hash_ctx(void) {
    return (cx_hash_t *) &g_tx_ctx_current->fields_hash_ctx;
}

const s_tx_info *get_current_tx_info(void) {
    if (g_tx_ctx_current == NULL) return NULL;
    return g_tx_ctx_current->tx_info;
}

const s_tx_info *get_root_tx_info(void) {
    if (g_tx_ctx_list == NULL) return NULL;
    return g_tx_ctx_list->tx_info;
}

s_calldata *get_current_calldata(void) {
    if (g_tx_ctx_current == NULL) return NULL;
    return g_tx_ctx_current->calldata;
}

s_calldata *get_root_calldata(void) {
    if (g_tx_ctx_list == NULL) return NULL;
    return g_tx_ctx_list->calldata;
}

const uint8_t *get_current_tx_from(void) {
    if (g_tx_ctx_current == NULL) return NULL;
    return g_tx_ctx_current->from;
}

const uint8_t *get_current_tx_to(void) {
    if (g_tx_ctx_current == NULL) return NULL;
    return g_tx_ctx_current->to;
}

const uint8_t *get_current_tx_amount(void) {
    if (g_tx_ctx_current == NULL) return NULL;
    return g_tx_ctx_current->amount;
}

uint64_t get_current_tx_chain_id(void) {
    if (g_tx_ctx_current == NULL) return 0;
    return g_tx_ctx_current->chain_id;
}

static bool validate_inst_hash_on(const s_tx_ctx *tx_ctx) {
    cx_sha3_t hash_ctx;
    uint8_t hash[sizeof(tx_ctx->tx_info->fields_hash)];

    if ((tx_ctx == NULL) || (tx_ctx->tx_info == NULL)) return false;
    // copy it locally, because the cx_hash call will modify it
    memcpy(&hash_ctx, &tx_ctx->fields_hash_ctx, sizeof(hash_ctx));
    if (finalize_hash((cx_hash_t *) &hash_ctx, hash, sizeof(hash)) != true) {
        return false;
    }
    return memcmp(tx_ctx->tx_info->fields_hash, hash, sizeof(hash)) == 0;
}

bool validate_instruction_hash(void) {
    return validate_inst_hash_on(g_tx_ctx_current);
}

static void delete_tx_ctx(s_tx_ctx *node) {
    if (node->tx_info != NULL) {
        delete_tx_info(node->tx_info);
    }
    if (node->calldata != NULL) {
        if (g_parked_calldata == node->calldata) {
            g_parked_calldata = NULL;
        }
        calldata_delete(node->calldata);
    }
    APP_MEM_FREE(node);
}

void tx_ctx_pop(void) {
    list_node_t *old_current = (list_node_t *) g_tx_ctx_current;

    g_tx_ctx_current = (s_tx_ctx *) ((list_node_t *) g_tx_ctx_current)->prev;
    list_remove((list_node_t **) &g_tx_ctx_list, old_current, (f_list_node_del) &delete_tx_ctx);
}

static bool process_empty_tx(const s_tx_ctx *tx_ctx) {
    const char *ticker;
    uint8_t decimals;
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);
    e_param_type param_type;

    if (tx_ctx->has_amount) {
        if (!set_intent_field("Send")) {
            return false;
        }
        // The nested context carries its own (possibly cross-chain) chain ID
        ticker = get_displayable_ticker(&tx_ctx->chain_id, g_chain_config, true);
        decimals = WEI_TO_ETHER;
        if (!amountToString(tx_ctx->amount,
                            sizeof(tx_ctx->amount),
                            decimals,
                            ticker,
                            buf,
                            buf_size)) {
            return false;
        }
        if (!add_to_field_table(PARAM_TYPE_AMOUNT, "Amount", buf, NULL)) {
            return false;
        }
    } else {
        if (!set_intent_field("Empty transaction")) {
            return false;
        }
    }

    e_name_type types[] = {TN_TYPE_ACCOUNT};
    e_name_source sources[] = {TN_SOURCE_ENS, TN_SOURCE_LAB, TN_SOURCE_MAB};
    e_addr_name_source name_source;
    const void *extra_data = NULL;

    // Address Book > Trusted Name > raw hex; extra_data carries the matched record for the alias
    // detail view
    if (!get_address_display_name(tx_ctx->to,
                                  tx_ctx->chain_id,
                                  ARRAYLEN(types),
                                  types,
                                  ARRAYLEN(sources),
                                  sources,
                                  buf,
                                  buf_size,
                                  &name_source,
                                  &extra_data)) {
        return false;
    }
    // param_type drives how the field is rendered: ADDRESS_BOOK and TRUSTED_NAME
    // entries carry an alias detail view (tappable name), RAW shows the plain address
    switch (name_source) {
        case ADDR_NAME_FROM_ADDRESS_BOOK:
            param_type = PARAM_TYPE_ADDRESS_BOOK;
            break;
        case ADDR_NAME_FROM_TRUSTED_NAME:
            param_type = PARAM_TYPE_TRUSTED_NAME;
            break;
        default:
            param_type = PARAM_TYPE_RAW;
            break;
    }
    if (!add_to_field_table(param_type, "To", buf, extra_data)) {
        return false;
    }
    // A call on another chain than the root one is told apart by its network
    if ((g_tx_ctx_list == NULL) || (tx_ctx->chain_id != g_tx_ctx_list->chain_id)) {
        if (!get_network_as_string_from_chain_id(buf, buf_size, tx_ctx->chain_id) ||
            !add_to_field_table(PARAM_TYPE_RAW, "Network", buf, NULL)) {
            return false;
        }
    }
    list_remove((list_node_t **) &g_tx_ctx_list,
                (list_node_t *) tx_ctx,
                (f_list_node_del) &delete_tx_ctx);
    return true;
}

bool process_empty_txs_before(void) {
    list_node_t *tmp = ((list_node_t *) g_tx_ctx_current)->prev;
    while ((tmp != NULL) && (((s_tx_ctx *) tmp)->calldata == NULL)) {
        // process_empty_tx calls list_remove + delete_tx_ctx, which frees tmp.
        // Ensure reading tmp->prev before the call to avoid use-after-free.
        list_node_t *prev = tmp->prev;
        if (!process_empty_tx((s_tx_ctx *) tmp)) {
            return false;
        }
        tmp = prev;
    }
    return true;
}

bool process_empty_txs_after(void) {
    flist_node_t *tmp = ((flist_node_t *) g_tx_ctx_current)->next;
    while ((tmp != NULL) && (((s_tx_ctx *) tmp)->calldata == NULL)) {
        // process_empty_tx calls list_remove + delete_tx_ctx, which frees tmp.
        // Ensure reading tmp->next before the call to avoid use-after-free.
        flist_node_t *next = tmp->next;
        if (!process_empty_tx((s_tx_ctx *) tmp)) {
            return false;
        }
        tmp = next;
    }
    return true;
}

bool find_matching_tx_ctx(const uint8_t *contract_addr,
                          const uint8_t *selector,
                          const uint64_t *chain_id) {
    const uint8_t *proxy_implem;
    s_tx_ctx *expected = NULL;

    // Only the next undescribed context in execution order is eligible, so the
    // host cannot reorder the review vs the actual call sequence. Empty
    // transactions have no TX_INFO and are handled by process_empty_txs_*().
    for (s_tx_ctx *tmp = g_tx_ctx_list; tmp != NULL;
         tmp = (s_tx_ctx *) ((flist_node_t *) tmp)->next) {
        if ((tmp->tx_info == NULL) && (tmp->calldata != NULL)) {
            expected = tmp;
            break;
        }
    }
    if (expected == NULL) {
        return false;
    }

    proxy_implem = get_implem_contract(chain_id, expected->to, selector);
    if ((memcmp((proxy_implem != NULL) ? proxy_implem : expected->to,
                contract_addr,
                ADDRESS_LENGTH) == 0) &&
        (memcmp(selector, expected->calldata->selector, CALLDATA_SELECTOR_SIZE) == 0) &&
        (*chain_id == expected->chain_id)) {
        g_tx_ctx_current = expected;
        return true;
    }
    return false;
}

static void tx_ctx_cleanup(void) {
    list_clear((list_node_t **) &g_tx_ctx_list, (f_list_node_del) &delete_tx_ctx);
    g_tx_ctx_current = NULL;
}

bool set_tx_info_into_tx_ctx(s_tx_info *tx_info) {
    cx_sha3_t ctx;
    uint8_t hash[INT256_LENGTH];

    if (g_tx_ctx_current == NULL) return false;
    g_tx_ctx_current->tx_info = tx_info;
    if (tx_ctx_is_root()) {
        if (appState == APP_STATE_SIGNING_EIP712) {
            if (!set_intent_field(tx_info->operation_type)) return false;
        }
    } else {
        if (!set_intent_field(tx_info->operation_type)) return false;
    }

    if ((appState == APP_STATE_SIGNING_EIP712) || !tx_ctx_is_root()) {
        if (cx_sha3_init_no_throw(&ctx, 256) != CX_OK) {
            return false;
        }
        if (finalize_hash((cx_hash_t *) &ctx, hash, sizeof(hash)) != true) {
            return false;
        }
        if (memcmp(hash, tx_info->fields_hash, sizeof(hash)) == 0) {
            tx_ctx_pop();
        }
    }
    return true;
}

bool tx_ctx_init(s_calldata *calldata,
                 const uint8_t *from,
                 const uint8_t *to,
                 const uint8_t *amount,
                 const uint64_t *chain_id) {
    s_tx_ctx *node;
    s_eip712_calldata_info *calldata_info;

    if (APP_MEM_CALLOC((void **) &node, sizeof(*node)) == false) {
        return false;
    }
    node->calldata = calldata;
    if (get_tx_ctx_count() == 0) {
        get_public_key(node->from, ADDRESS_LENGTH);
        if (appState == APP_STATE_SIGNING_EIP712) {
            calldata_info = get_current_calldata_info();
            if (!calldata_info_all_received(calldata_info) || calldata_info->processed) {
                APP_MEM_FREE(node);
                return false;
            }
            calldata_info->processed = true;
        }
    } else {
        // Inherit defaults from the logical parent: the context being
        // formatted when known, the list tail at queue time (creation order
        // matches nesting order before any matching has run).
        const s_tx_ctx *parent = g_tx_ctx_current;
        if (parent == NULL) {
            parent = g_tx_ctx_list;
            while (((const flist_node_t *) parent)->next != NULL) {
                parent = (const s_tx_ctx *) ((const flist_node_t *) parent)->next;
            }
        }
        memcpy(node->from, parent->from, sizeof(node->from));
        node->chain_id = parent->chain_id;
    }

    if (from != NULL) {
        memcpy(node->from, from, sizeof(node->from));
    }
    if (to != NULL) {
        memcpy(node->to, to, sizeof(node->to));
    }
    if (amount != NULL) {
        memcpy(node->amount, amount, sizeof(node->amount));
        node->has_amount = true;
    }
    if (chain_id != NULL) {
        node->chain_id = *chain_id;
    }

    if (cx_sha3_init_no_throw(&node->fields_hash_ctx, 256) != CX_OK) {
        APP_MEM_FREE(node);
        return false;
    }
    list_push_back((list_node_t **) &g_tx_ctx_list, (list_node_t *) node);

    // Ownership of the calldata has been transferred to the node.
    // Clear g_parked_calldata now so callers cannot double-free it if we return
    // false below (e.g. when field_table_init fails after the node is in the list
    // and will be freed by tx_ctx_cleanup via delete_tx_ctx).
    if (g_parked_calldata == calldata) {
        g_parked_calldata = NULL;
    }

    if ((appState == APP_STATE_SIGNING_TX) && (node == g_tx_ctx_list)) {
        return field_table_init();
    }
    return true;
}

void gcs_set_root_value_shown(void) {
    g_root_value_shown = true;
}

bool gcs_is_root_value_shown(void) {
    return g_root_value_shown;
}

void gcs_set_root_from_shown(void) {
    g_root_from_shown = true;
}

bool gcs_is_root_from_shown(void) {
    return g_root_from_shown;
}

void gcs_cleanup(void) {
    ui_gcs_cleanup();
    field_table_cleanup();
    tx_ctx_cleanup();
    g_root_value_shown = false;
    g_root_from_shown = false;
    // just in case
    if (g_parked_calldata != NULL) {
        calldata_delete(g_parked_calldata);
        g_parked_calldata = NULL;
    }
}
