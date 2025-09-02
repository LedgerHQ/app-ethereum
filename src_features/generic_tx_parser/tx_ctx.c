#include "tx_ctx.h"
#include "mem.h"
#include "gtp_field_table.h"
#include "common_ui.h" // ui_gcs_cleanup

static s_tx_ctx *g_tx_ctx_list = NULL;
static s_tx_ctx *g_tx_ctx_current = NULL;

const s_tx_ctx *get_current_tx_ctx(void) {
    return g_tx_ctx_current;
}

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

static bool validate_inst_hash_on(const s_tx_ctx *tx_ctx) {
    cx_sha3_t hash_ctx;
    uint8_t hash[sizeof(tx_ctx->tx_info->fields_hash)];

    if ((tx_ctx == NULL) || (tx_ctx->tx_info == NULL)) return false;
    // copy it locally, because the cx_hash call will modify it
    memcpy(&hash_ctx, &tx_ctx->fields_hash_ctx, sizeof(hash_ctx));
    if (cx_hash_no_throw((cx_hash_t *) &hash_ctx,
                         CX_LAST,
                         NULL,
                         0,
                         hash,
                         sizeof(hash)) != CX_OK) {
        return false;
    }
    return memcmp(tx_ctx->tx_info->fields_hash, hash, sizeof(hash)) == 0;
}

bool validate_instruction_hash(void) {
    return validate_inst_hash_on(g_tx_ctx_current);
}

bool push_field_into_tx_ctx(const s_field *field) {
    s_field_list_node *node;

    if (g_tx_ctx_current == NULL) return false;
    if ((node = app_mem_alloc(sizeof(*node))) == NULL) {
        return false;
    }
    explicit_bzero(node, sizeof(*node));
    memcpy(&node->field, field, sizeof(*field));
    flist_push_back((s_flist_node **) &g_tx_ctx_current->fields, (s_flist_node *) node);
    return true;
}

// TODO: make doubly linked
// g_tx_ctx_current = g_tx_ctx_current->prev;
void tx_ctx_move_to_parent(void) {
    s_flist_node *tmp = (s_flist_node *) g_tx_ctx_list;

    while ((tmp != NULL) && (tmp->next != (s_flist_node *) g_tx_ctx_current)) {
        tmp = tmp->next;
    }
    g_tx_ctx_current = (s_tx_ctx *) tmp;
}

bool find_matching_tx_ctx(const uint8_t *contract_addr,
                          const uint8_t *selector,
                          const uint64_t *chain_id) {
    for (s_tx_ctx *tmp = g_tx_ctx_list;
         tmp != NULL;
         tmp = (s_tx_ctx *) ((s_flist_node *) tmp)->next) {
        if ((memcmp(contract_addr, tmp->tx_info->contract_addr, ADDRESS_LENGTH) == 0) &&
            (memcmp(selector, tmp->tx_info->selector, CALLDATA_SELECTOR_SIZE) == 0) &&
            (*chain_id == tmp->tx_info->chain_id)) {
            g_tx_ctx_current = tmp;
            return true;
        }
    }
    return false;
}

s_field_list_node *get_fields_list(void) {
    if (g_tx_ctx_current == NULL) return NULL;
    return g_tx_ctx_current->fields;
}

static void delete_field(s_field_list_node *node) {
    app_mem_free(node);
}

static void delete_tx_ctx(s_tx_ctx *node) {
    if (node->tx_info != NULL) {
        delete_tx_info(node->tx_info);
    }
    if (node->calldata != NULL) {
        delete_calldata(node->calldata);
    }
    if (node->fields != NULL) {
        flist_clear((s_flist_node **) &node->fields, (f_list_node_del) &delete_field);
    }
}

void tx_ctx_cleanup(void) {
    flist_clear((s_flist_node **) &g_tx_ctx_list, (f_list_node_del) &delete_tx_ctx);
    g_tx_ctx_current = NULL;
}

bool new_tx_ctx(s_tx_info *tx_info) {
    s_tx_ctx *node;
    if ((node = app_mem_alloc(sizeof(*node))) == NULL) {
        return false;
    }
    explicit_bzero(node, sizeof(*node));
    node->tx_info = tx_info;
    if (cx_sha3_init_no_throw(&node->fields_hash_ctx, 256) != CX_OK) {
        return false;
    }
    flist_push_back((s_flist_node **) &g_tx_ctx_list, (s_flist_node *) node);
    if (node == g_tx_ctx_list) {
        if (!field_table_init()) return false;
    }
    g_tx_ctx_current = node;
    return true;
}

void gcs_cleanup(void) {
    ui_gcs_cleanup();
    field_table_cleanup();
    tx_ctx_cleanup();
    //calldata_cleanup();
}
