#include "tx_ctx.h"

static s_tx_ctx *g_tx_ctx = NULL;

const s_tx_ctx *get_current_tx_ctx(void) {
    s_flist_node *tmp = (s_flist_node *)g_tx_ctx;

    while ((tmp != NULL) && (tmp->next != NULL)) tmp = tmp->next;
    return (s_tx_ctx *)tmp;
}
