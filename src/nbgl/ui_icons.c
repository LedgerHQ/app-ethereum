#include "os_utils.h"
#include "os_pic.h"
#include "ui_nbgl.h"
#include "ui_icons.h"
#include "caller_app.h"
#include "plugins.h"
#include "network.h"
#include "network_info.h"
#ifdef SCREEN_SIZE_WALLET
#include "net_icons.gen.h"
#endif

/**
 * Retrieve the app icon, using the caller app icon if requested
 *
 * @param[in] caller_icon If true, use the caller app icon if available
 * @return Pointer to the icon details structure
 */
const nbgl_icon_details_t *get_app_icon(bool caller_icon) {
    // Plugin or clone case: prefer the caller app's own icon
    if (caller_icon && g_caller_app) {
        if (g_caller_app->icon) {
            return g_caller_app->icon;
        }
        PRINTF("%s: g_caller_app has no icon\n", __func__);
    }
    // Default: Ethereum app icon
    return &ICONGLYPH;
}

/**
 * Retrieve the home screen icon, using the caller app icon if available
 *
 * @return Pointer to the icon details structure
 */
const nbgl_icon_details_t *get_home_icon(void) {
    // Plugin or clone case: prefer the caller app's own icon
    if (g_caller_app) {
        if (g_caller_app->icon) {
            return g_caller_app->icon;
        }
        PRINTF("%s: g_caller_app has no icon\n", __func__);
    }
    // Default: Ethereum home icon
    return &ICONHOME;
}

/**
 * Retrieve the icon for the Transaction
 *
 * @param[in] fromPlugin If true, the data is coming from a plugin, otherwise it is a standard
 * transaction
 * @return Pointer to the icon details structure, or NULL if no icon is available
 */
const nbgl_icon_details_t *get_tx_icon(bool fromPlugin) {
    if (fromPlugin && (pluginType == PLUGIN_TYPE_EXTERNAL)) {
        if ((g_caller_app != NULL) && (g_caller_app->name != NULL)) {
            if (strcmp(strings.common.toAddress, g_caller_app->name) == 0) {
                return get_app_icon(true);
            }
        }
        // icon is NULL in this case
        // Check with Alex if this is expected or a bug
        return NULL;
    }
    if ((g_caller_app != NULL) && !fromPlugin) {
        // Clone case
        return get_app_icon(true);
    }
    // Standard transaction: use app icon if chain matches, else network icon
    uint64_t chain_id = get_tx_chain_id();
    if (chain_id == g_chain_config->chain_id) {
        return get_app_icon(false);
    }
    return get_network_icon_from_chain_id(&chain_id);
}

/**
 * Get the network icon from a given chain ID
 *
 * Loops onto the generated \ref g_network_icons array until a chain ID matches.
 *
 * @param[in] chain_id network's chain ID
 * @return the network icon if found, \ref NULL otherwise
 */
const nbgl_icon_details_t *get_network_icon_from_chain_id(const uint64_t *chain_id) {
    // Search in dynamically loaded networks
    network_info_t *net_info = find_dynamic_network_by_chain_id(*chain_id);
    if (net_info != NULL && net_info->icon.bitmap != NULL) {
        PRINTF("[NETWORK_ICONS] - Found dynamic '%s'\n", net_info->name);
        return PIC(&net_info->icon);
    }
#ifdef SCREEN_SIZE_WALLET
    for (size_t i = 0; i < ARRAYLEN(g_network_icons); ++i) {
        if ((uint64_t) PIC(g_network_icons[i].chain_id) == *chain_id) {
            PRINTF("[NETWORK_ICONS] - Fallback on hardcoded list.\n");
            return PIC(g_network_icons[i].icon);
        }
    }
#else
    // Nano devices don't have the array of icons, fallback on the app's icon
    if (*chain_id == ETHEREUM_MAINNET_CHAINID) {
        return &ICONGLYPH;
    }
#endif
    return NULL;
}

/**
 * Get the icon for a clone network
 *
 * @param[in] caller_app Pointer to the caller app structure
 * @return Pointer to the icon details structure, or NULL if not a clone
 */
const nbgl_icon_details_t *get_clone_network_icon(const caller_app_t *caller_app) {
    if ((caller_app == NULL) || (caller_app->type != CALLER_TYPE_CLONE)) {
        return NULL;
    }
    return caller_app->icon;
}
