#include "tx_ctx.h"
#include "mem.h"
#include "gtp_field_table.h"
#include "proxy_info.h"
#include "common_ui.h"       // ui_gcs_cleanup
#include "shared_context.h"  // appState
#include "utils.h"           // buf_shrink_expand
#include "getPublicKey.h"
#include "context_712.h"
#include "network.h"
#include "hash_bytes.h"

static s_tx_ctx *g_tx_ctx_list = NULL;
static s_tx_ctx *g_tx_ctx_current = NULL;
s_calldata *g_parked_calldata = NULL;

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
        calldata_delete(node->calldata);
    }
    app_mem_free(node);
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
    const s_tx_info *tx_info = tx_ctx->tx_info;
    e_param_type param_type;
    const s_trusted_name *trusted_name = NULL;

    if (tx_ctx->has_amount) {
        if (!set_intent_field("Send")) {
            return false;
        }
        if (tx_info == NULL) {
            if ((tx_info = get_root_tx_info()) == NULL) {
                return false;
            }
        }
        ticker = get_displayable_ticker(&tx_info->chain_id, chainConfig, true);
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

    uint64_t chain_id = get_tx_chain_id();
    e_name_type types[] = {TN_TYPE_ACCOUNT};
    e_name_source sources[] = {TN_SOURCE_ENS, TN_SOURCE_LAB, TN_SOURCE_MAB};

    if ((trusted_name = get_trusted_name(ARRAYLEN(types),
                                         types,
                                         ARRAYLEN(sources),
                                         sources,
                                         &chain_id,
                                         tx_ctx->to)) != NULL) {
        param_type = PARAM_TYPE_TRUSTED_NAME;
        strlcpy(buf, trusted_name->name, buf_size);
    } else {
        param_type = PARAM_TYPE_RAW;
        if (!getEthDisplayableAddress(tx_ctx->to, buf, buf_size, chainConfig->chainId)) {
            return false;
        }
    }
    if (!add_to_field_table(param_type, "To", buf, trusted_name)) {
        return false;
    }
    list_remove((list_node_t **) &g_tx_ctx_list,
                (list_node_t *) tx_ctx,
                (f_list_node_del) &delete_tx_ctx);
    return true;
}

bool process_empty_txs_before(void) {
    for (list_node_t *tmp = ((list_node_t *) g_tx_ctx_current)->prev;
         (tmp != NULL) && (((s_tx_ctx *) tmp)->calldata == NULL);
         tmp = tmp->prev) {
        if (!process_empty_tx((s_tx_ctx *) tmp)) {
            return false;
        }
    }
    return true;
}

bool process_empty_txs_after(void) {
    for (flist_node_t *tmp = ((flist_node_t *) g_tx_ctx_current)->next;
         (tmp != NULL) && (((s_tx_ctx *) tmp)->calldata == NULL);
         tmp = tmp->next) {
        if (!process_empty_tx((s_tx_ctx *) tmp)) {
            return false;
        }
    }
    return true;
}

bool find_matching_tx_ctx(const uint8_t *contract_addr,
                          const uint8_t *selector,
                          const uint64_t *chain_id) {
    const uint8_t *proxy_implem;

    for (s_tx_ctx *tmp = g_tx_ctx_list; tmp != NULL;
         tmp = (s_tx_ctx *) ((flist_node_t *) tmp)->next) {
        proxy_implem = get_implem_contract(chain_id, tmp->to, selector);
        if ((memcmp((proxy_implem != NULL) ? proxy_implem : tmp->to,
                    contract_addr,
                    ADDRESS_LENGTH) == 0) &&
            ((tmp->calldata != NULL) &&
             (memcmp(selector, tmp->calldata->selector, CALLDATA_SELECTOR_SIZE) == 0)) &&
            (*chain_id == tmp->chain_id)) {
            g_tx_ctx_current = tmp;
            return true;
        }
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
        while (((const flist_node_t *) tmp)->next != NULL) {
            tmp = (const s_tx_ctx *) ((const flist_node_t *) tmp)->next;
        }
        memcpy(node->from, tmp->from, sizeof(node->from));
        node->chain_id = tmp->chain_id;
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
        app_mem_free(node);
        return false;
    }
    list_push_back((list_node_t **) &g_tx_ctx_list, (list_node_t *) node);
    if ((appState == APP_STATE_SIGNING_TX) && (node == g_tx_ctx_list)) {
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
