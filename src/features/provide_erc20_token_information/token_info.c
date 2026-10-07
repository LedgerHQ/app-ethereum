#include <string.h>
#include "lists.h"
#include "app_mem_utils.h"
#include "token_info.h"
#include "network.h"

typedef struct {
    flist_node_t _list;
    s_token_info info;
} s_token_info_node;

static s_token_info_node *g_token_info_list;

int set_token_info(const s_token_info *info) {
    s_token_info_node *node;
    int idx = 0;

    if (info == NULL) {
        return -1;
    }
    // look for an existing matching node
    for (node = g_token_info_list; node != NULL;
         node = (s_token_info_node *) ((flist_node_t *) node)->next) {
        if (info->chain_id == node->info.chain_id) {
            if (memcmp(info->address, node->info.address, sizeof(node->info.address)) == 0) {
                break;
            }
        }
        idx += 1;
    }

    if (node == NULL) {
        if ((node = APP_MEM_ALLOC(sizeof(*node))) == NULL) {
            return -1;
        }
        flist_push_back((flist_node_t **) &g_token_info_list, (flist_node_t *) node);
    }
    memcpy(&node->info, info, sizeof(node->info));
    return idx;
}

static void delete_token_info(s_token_info_node *node) {
    APP_MEM_FREE(node);
}

void clear_token_infos(void) {
    flist_clear((flist_node_t **) &g_token_info_list, (f_list_node_del) &delete_token_info);
}

const s_token_info *get_matching_token_info(const uint64_t *chain_id, const uint8_t *address) {
    if ((chain_id != NULL) && (address != NULL)) {
        for (const s_token_info_node *node = g_token_info_list; node != NULL;
             node = (s_token_info_node *) ((flist_node_t *) node)->next) {
            if (*chain_id == node->info.chain_id) {
                if (memcmp(address, node->info.address, sizeof(node->info.address)) == 0) {
                    return &node->info;
                }
            }
        }
    }
    return NULL;
}

const s_token_info *get_matching_token_info_or_dummy(const uint64_t *chain_id,
                                                     const uint8_t *address) {
    const s_token_info *info_ptr;
    s_token_info info;

    if ((chain_id == NULL) || (address == NULL)) {
        return NULL;
    }
    if ((info_ptr = get_matching_token_info(chain_id, address)) != NULL) {
        return info_ptr;
    }
    info.chain_id = *chain_id;
    memcpy(info.address, address, ADDRESS_LENGTH);
    strlcpy(info.ticker, g_unknown_ticker, sizeof(info.ticker));
    info.decimals = 0;
    if (set_token_info(&info) == -1) {
        return NULL;
    }
    return get_matching_token_info(chain_id, address);
}
