#include "tx_ctx.h"
#include "mem.h"
#include "gtp_field_table.h"
#include "proxy_info.h"
#include "common_ui.h"       // ui_gcs_cleanup
#include "shared_context.h"  // appState
#include "utils.h"           // buf_shrink_expand
#include "getPublicKey.h"
#include "context_712.h"

static s_tx_ctx *g_tx_ctx_list = NULL;
static s_tx_ctx *g_tx_ctx_current = NULL;
s_calldata *g_parked_calldata = NULL;

bool tx_ctx_is_root(void) {
    return (g_tx_ctx_list != NULL) && (g_tx_ctx_current == g_tx_ctx_list);
}

size_t get_tx_ctx_count(void) {
    return flist_size((s_flist_node **) &g_tx_ctx_list);
}

cx_hash_t *get_fields_hash_ctx(void) {
    return (cx_hash_t *) &g_tx_ctx_current->fields_hash_ctx;
}

const s_tx_info *get_current_tx_info(void) {
    if (g_tx_ctx_current == NULL) return NULL;
    return g_tx_ctx_current->tx_info;
}

s_calldata *get_current_calldata(void) {
    if (g_tx_ctx_current == NULL) return NULL;
    return g_tx_ctx_current->calldata;
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

static bool validate_inst_hash_on(const s_tx_ctx *tx_ctx) {
    cx_sha3_t hash_ctx;
    uint8_t hash[sizeof(tx_ctx->tx_info->fields_hash)];

    if ((tx_ctx == NULL) || (tx_ctx->tx_info == NULL)) return false;
    // copy it locally, because the cx_hash call will modify it
    memcpy(&hash_ctx, &tx_ctx->fields_hash_ctx, sizeof(hash_ctx));
    if (cx_hash_no_throw((cx_hash_t *) &hash_ctx, CX_LAST, NULL, 0, hash, sizeof(hash)) != CX_OK) {
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
        calldata_delete(node->calldata);
    }
    app_mem_free(node);
}

void tx_ctx_pop(void) {
    s_flist_node *old_current = (s_flist_node *) g_tx_ctx_current;

    // TODO: make doubly linked to simply get the prev one
    for (s_flist_node *tmp = (s_flist_node *) g_tx_ctx_list; tmp != NULL; tmp = tmp->next) {
        if (tmp->next == old_current) {
            g_tx_ctx_current = (s_tx_ctx *) tmp;
            break;
        }
    }
    if (g_tx_ctx_current == (s_tx_ctx *) old_current) {
        // there was no previous one
        // there might still be some elements in the list but after the one that we're removing
        g_tx_ctx_current = NULL;
    }
    flist_remove((s_flist_node **) &g_tx_ctx_list, old_current, (f_list_node_del) &delete_tx_ctx);
}

bool find_matching_tx_ctx(const uint8_t *contract_addr,
                          const uint8_t *selector,
                          const uint64_t *chain_id) {
    const uint8_t *proxy_implem;

    for (s_tx_ctx *tmp = g_tx_ctx_list; tmp != NULL;
         tmp = (s_tx_ctx *) ((s_flist_node *) tmp)->next) {
        proxy_implem = get_implem_contract(chain_id, tmp->to, selector);
        if ((memcmp((proxy_implem != NULL) ? proxy_implem : tmp->to,
                    contract_addr,
                    ADDRESS_LENGTH) == 0) &&
            (memcmp(selector, tmp->calldata->selector, CALLDATA_SELECTOR_SIZE) == 0) &&
            (*chain_id == tmp->chain_id)) {
            g_tx_ctx_current = tmp;
            return true;
        }
    }
    return false;
}

void tx_ctx_cleanup(void) {
    flist_clear((s_flist_node **) &g_tx_ctx_list, (f_list_node_del) &delete_tx_ctx);
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
        if (cx_hash_no_throw((cx_hash_t *) &ctx, CX_LAST, NULL, 0, hash, sizeof(hash)) != CX_OK) {
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

    if (calldata == NULL) return false;
    if ((node = app_mem_alloc(sizeof(*node))) == NULL) {
        return false;
    }
    explicit_bzero(node, sizeof(*node));
    node->calldata = calldata;
    if (get_tx_ctx_count() == 0) {
        get_public_key(node->from, ADDRESS_LENGTH);
        if (appState == APP_STATE_SIGNING_EIP712) {
            calldata_info = get_current_calldata_info();
            if (!calldata_info_all_received(calldata_info) || calldata_info->processed) {
                app_mem_free(node);
                return false;
            }
            calldata_info->processed = true;
        }
    } else {
        // as default, copy value from last tx context
        const s_tx_ctx *tmp = g_tx_ctx_list;
        while (((const s_flist_node *) tmp)->next != NULL) {
            tmp = (const s_tx_ctx *) ((const s_flist_node *) tmp)->next;
        }
        memcpy(node->from, tmp->from, sizeof(node->from));
        memcpy(node->to, tmp->to, sizeof(node->to));
        memcpy(node->amount, tmp->amount, sizeof(node->amount));
        node->chain_id = tmp->chain_id;
    }

    if (from != NULL) memcpy(node->from, from, sizeof(node->from));
    if (to != NULL) memcpy(node->to, to, sizeof(node->to));
    if (amount != NULL) memcpy(node->amount, amount, sizeof(node->amount));
    if (chain_id != NULL) node->chain_id = *chain_id;

    if (cx_sha3_init_no_throw(&node->fields_hash_ctx, 256) != CX_OK) {
        app_mem_free(node);
        return false;
    }
    flist_push_back((s_flist_node **) &g_tx_ctx_list, (s_flist_node *) node);
    g_tx_ctx_current = node;
    if ((appState == APP_STATE_SIGNING_TX) && tx_ctx_is_root()) {
        return field_table_init();
    }
    return true;
}

void gcs_cleanup(void) {
    ui_gcs_cleanup();
    field_table_cleanup();
    tx_ctx_cleanup();
    // just in case
    if (g_parked_calldata != NULL) calldata_delete(g_parked_calldata);
}
