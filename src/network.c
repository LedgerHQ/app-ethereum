#include "os_utils.h"
#include "os_pic.h"
#include "network.h"
#include "network_info.h"
#include "shared_context.h"
#include "common_utils.h"
#include "apdu_constants.h"

const char g_unknown_ticker[] = "???";

/**
 * @brief Find a dynamically loaded network by its chain ID
 *
 * @param[in] chain_id The chain ID to search for
 * @return Pointer to network_info_t if found, NULL otherwise
 */
network_info_t *find_dynamic_network_by_chain_id(uint64_t chain_id) {
    flist_node_t *node = (flist_node_t *) g_dynamic_network_list;
    while (node != NULL) {
        network_info_t *net_info = (network_info_t *) node;
        if (net_info->chain_id == chain_id) {
            return net_info;
        }
        node = node->next;
    }
    return NULL;
}

static const network_info_t *get_network_from_chain_id(const uint64_t *chain_id) {
    if (*chain_id != 0) {
        // Look if the network is available in dynamically loaded networks
        network_info_t *net_info = find_dynamic_network_by_chain_id(*chain_id);
        if (net_info != NULL) {
            PRINTF("[NETWORK] - Found dynamic '%s'\n", net_info->name);
            return (const network_info_t *) net_info;
        }
    }
    return NULL;
}

static const char *get_network_ticker_from_chain_id(const uint64_t *chain_id) {
    const network_info_t *net = get_network_from_chain_id(chain_id);

    if (net == NULL) {
        return NULL;
    }
    return PIC(net->ticker);
}

const char *get_network_name_from_chain_id(const uint64_t *chain_id) {
    const network_info_t *net = get_network_from_chain_id(chain_id);

    if (net == NULL) {
        // No dynamic network found: fall back to the app's own name (APPNAME,
        // from the build config) for its native chain, since static networks are
        // no longer embedded and the app does not load itself as a dynamic network.
        if (*chain_id == g_chain_config->chain_id) {
            return APPNAME;
        }
        return NULL;
    }
    return PIC(net->name);
}

bool get_network_as_string_from_chain_id(char *out, size_t out_size, uint64_t chain_id) {
    const char *name = get_network_name_from_chain_id(&chain_id);

    if (name == NULL) {
        // No network name found so simply copy the chain ID as the network name.
        if (!format_u64(out, out_size, chain_id)) {
            return false;
        }
    } else {
        // Network name found, simply copy it.
        strlcpy(out, name, out_size);
    }
    return true;
}

bool get_network_as_string(char *out, size_t out_size) {
    uint64_t chain_id = get_tx_chain_id();
    return get_network_as_string_from_chain_id(out, out_size, chain_id);
}

bool chain_is_ethereum_compatible(const uint64_t *chain_id) {
    // The application always supports its own (build-time) chain, in addition to
    // any dynamically loaded network.
    if (*chain_id == g_chain_config->chain_id) {
        return true;
    }
    return get_network_from_chain_id(chain_id) != NULL;
}

// Returns the chain ID. Defaults to 0 if txType was not found (For TX).
uint64_t get_tx_chain_id(void) {
    uint64_t chain_id = 0;

    switch (txContext.txType) {
        case LEGACY:
            chain_id = u64_from_BE(txContext.content->v, txContext.content->vLength);
            break;
        case EIP2930:
        case EIP1559:
        case EIP7702:
            chain_id = u64_from_BE(tmpContent.txContent.chainID.value,
                                   tmpContent.txContent.chainID.length);
            break;
        default:
            PRINTF("Txtype `%d` not supported while generating chainID\n", txContext.txType);
            break;
    }
    return chain_id;
}

const char *get_displayable_ticker(const uint64_t *chain_id, const chain_config_t *chain_cfg) {
    const char *ticker = get_network_ticker_from_chain_id(chain_id);

    if (ticker == NULL) {
        if (*chain_id == chain_cfg->chain_id) {
            ticker = chain_cfg->ticker;
        } else {
            ticker = g_unknown_ticker;
        }
    }
    return ticker;
}

/**
 * Checks whether the app can support the given chain ID
 *
 * - If the given chain ID is the same as the app's one
 * - If both chain IDs are present in the array of Ethereum-compatible networks
 */
bool app_compatible_with_chain_id(const uint64_t *chain_id) {
    return ((g_chain_config->chain_id == *chain_id) ||
            (chain_is_ethereum_compatible(&g_chain_config->chain_id) &&
             chain_is_ethereum_compatible(chain_id)));
}

const char *get_clone_network_name(const caller_app_t *caller_app) {
    if ((caller_app == NULL) || (caller_app->type != CALLER_TYPE_CLONE)) {
        return NULL;
    }
    return caller_app->name;
}
