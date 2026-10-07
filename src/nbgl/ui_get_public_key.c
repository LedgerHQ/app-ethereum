#include "shared_context.h"
#include "apdu_constants.h"
#include "common_ui.h"
#include "ui_nbgl.h"
#include "network.h"
#include "ui_icons.h"
#include "ui_utils.h"

static void review_choice(bool confirm) {
    if (confirm) {
        io_seproxyhal_touch_address_ok();
        nbgl_useCaseReviewStatus(STATUS_TYPE_ADDRESS_VERIFIED, ui_idle);
    } else {
        io_seproxyhal_touch_address_cancel();
        nbgl_useCaseReviewStatus(STATUS_TYPE_ADDRESS_REJECTED, ui_idle);
    }
    ui_buffers_cleanup();
}

void ui_display_public_key(const uint64_t *chain_id) {
    const nbgl_icon_details_t *icon = NULL;
    const char *title_prefix = "Verify ";
    const char *title_sufix = "address";
    const char *network_name = NULL;
    uint8_t title_len = 1;  // Initialize lengths to 1 for '\0' character

    // Compute the title message length
    title_len += strlen(title_prefix);

    if (g_chain_config->chain_id == ETHEREUM_MAINNET_CHAINID) {
        network_name = get_network_name_from_chain_id(chain_id);
        icon = get_network_icon_from_chain_id(chain_id);
    } else {
        network_name = get_clone_network_name(g_caller_app);
        icon = get_clone_network_icon(g_caller_app);
    }

    if (network_name != NULL) {
        title_len += strlen(network_name);
        title_len += 1;  // for '\n'
    }
    title_len += strlen(title_sufix);
    // Initialize the buffers
    if (!ui_buffers_init(title_len, 0, 0)) {
        // Initialization failed, cleanup and return
        io_seproxyhal_send_status(SWO_INSUFFICIENT_MEMORY, 0, true, true);
        return;
    }
    // Prepare the title message
    strlcpy(g_titleMsg, title_prefix, title_len);
    if (network_name != NULL) {
        strlcat(g_titleMsg, network_name, title_len);
        strlcat(g_titleMsg, "\n", title_len);
    }
    strlcat(g_titleMsg, title_sufix, title_len);

    nbgl_useCaseAddressReview(strings.common.toAddress,
                              NULL,
                              icon,
                              g_titleMsg,
                              NULL,
                              review_choice);
}
