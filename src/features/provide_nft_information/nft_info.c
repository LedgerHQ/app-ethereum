#include "app_mem_utils.h"
#include "lists.h"
#include "nft_info.h"

typedef struct {
    flist_node_t _list;
    s_nft_info info;
} s_nft_info_node;

static s_nft_info_node *g_nft_info_list;

int set_nft_info(const s_nft_info *info) {
    s_nft_info_node *node;
    int idx = 0;

    if (info == NULL) {
        return -1;
    }
    // look for an existing matching node
    for (node = g_nft_info_list; node != NULL;
         node = (s_nft_info_node *) ((flist_node_t *) node)->next) {
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
        flist_push_back((flist_node_t **) &g_nft_info_list, (flist_node_t *) node);
    }
    memcpy(&node->info, info, sizeof(node->info));
    return idx;
}

static void delete_nft_info(s_nft_info_node *node) {
    APP_MEM_FREE(node);
}

void clear_nft_infos(void) {
    flist_clear((flist_node_t **) &g_nft_info_list, (f_list_node_del) &delete_nft_info);
}

const s_nft_info *get_matching_nft_info(const uint64_t *chain_id, const uint8_t *address) {
    if ((chain_id != NULL) && (address != NULL)) {
        for (const s_nft_info_node *node = g_nft_info_list; node != NULL;
             node = (s_nft_info_node *) ((flist_node_t *) node)->next) {
            if (*chain_id == node->info.chain_id) {
                if (memcmp(address, node->info.address, sizeof(node->info.address)) == 0) {
                    return &node->info;
                }
            }
        }
    }
    return NULL;
}
