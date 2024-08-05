#include <nbgl_page.h>
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "network.h"
#include "network_icons.h"

static void cancel_send(void) {
    io_seproxyhal_touch_address_cancel();
}

static void confirm_send(void) {
    io_seproxyhal_touch_address_ok();
}

static void review_choice(bool confirm) {
    if (confirm) {
        nbgl_useCaseReviewStatus(STATUS_TYPE_ADDRESS_VERIFIED, confirm_send);
    } else {
        nbgl_useCaseReviewStatus(STATUS_TYPE_ADDRESS_REJECTED, cancel_send);
    }
}

void ui_display_public_key(const uint64_t *chain_id) {
    const nbgl_icon_details_t *icon;

    // - if a chain_id is given and it's - known, we specify its network name
    //                                   - unknown, we don't specify anything
    // - if no chain_id is given we specify the APPNAME (legacy behaviour)
    strlcpy(g_stax_shared_buffer, "Verify ", sizeof(g_stax_shared_buffer));
    if (chain_id != NULL) {
        if (chain_is_ethereum_compatible(chain_id)) {
            strlcat(g_stax_shared_buffer,
                    get_network_name_from_chain_id(chain_id),
                    sizeof(g_stax_shared_buffer));
            strlcat(g_stax_shared_buffer, "\n", sizeof(g_stax_shared_buffer));
        }
        icon = get_network_icon_from_chain_id(chain_id);
    } else {
        strlcat(g_stax_shared_buffer, APPNAME "\n", sizeof(g_stax_shared_buffer));
        icon = get_app_icon(false);
    }
    strlcat(g_stax_shared_buffer, "address", sizeof(g_stax_shared_buffer));
    nbgl_useCaseAddressReview(strings.common.toAddress,
                              NULL,
                              icon,
                              g_stax_shared_buffer,
                              NULL,
                              review_choice);
}
