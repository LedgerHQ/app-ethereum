#include "tx_ctx.h"
#include "mem.h"
#include "gtp_field_table.h"

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

bool new_tx_ctx(const s_tx_info *tx_info) {
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
