#include "os_utils.h"
#include "os_pic.h"
#include "net_icons.gen.h"

/**
 * Get the network icon from a given chain ID
 *
 * Loops onto the generated \ref g_network_icons array until a chain ID matches.
 *
 * @param[in] chain_id network's chain ID
 * @return the network icon if found, \ref NULL otherwise
 */
const nbgl_icon_details_t *get_network_icon_from_chain_id(const uint64_t *chain_id) {
    for (size_t i = 0; i < ARRAYLEN(g_network_icons); ++i) {
        if ((uint64_t) PIC(g_network_icons[i].chain_id) == *chain_id) {
            return PIC(g_network_icons[i].icon);
        }
    }
    return NULL;
}
