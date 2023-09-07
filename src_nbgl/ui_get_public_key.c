#include <nbgl_page.h>
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "network.h"

static void cancel_send(void) {
    io_seproxyhal_touch_address_cancel(NULL);
}

static void confirm_send(void) {
    io_seproxyhal_touch_address_ok(NULL);
}

static void confirm_addr(void) {
    // display a status page and go back to main
    nbgl_useCaseStatus("ADDRESS\nVERIFIED", true, confirm_send);
}

static void reject_addr(void) {
    nbgl_useCaseStatus("Address verification\ncancelled", false, cancel_send);
}

static void review_choice(bool confirm) {
    if (confirm) {
        confirm_addr();
    } else {
        reject_addr();
    }
}

static void display_addr(void) {
    nbgl_useCaseAddressConfirmation(strings.common.fullAddress, review_choice);
}

void ui_display_public_key(const uint64_t *chain_id) {
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
    } else {
        strlcat(g_stax_shared_buffer, APPNAME "\n", sizeof(g_stax_shared_buffer));
    }
    strlcat(g_stax_shared_buffer, "address", sizeof(g_stax_shared_buffer));
    nbgl_useCaseReviewStart(get_app_icon(false),
                            g_stax_shared_buffer,
                            NULL,
                            "Cancel",
                            display_addr,
                            reject_addr);
}
