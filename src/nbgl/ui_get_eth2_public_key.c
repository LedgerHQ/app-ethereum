#include "apdu_constants.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "ui_utils.h"

#define ETH2_ADDRESS_LENGTH 48
#define ETH2_ADDRESS_STR    (ETH2_ADDRESS_LENGTH * 2 + 3)  // '0x' + 48 bytes * 2 hex chars + '\0'

static void reviewChoice(bool confirm) {
    if (confirm) {
        io_seproxyhal_touch_address_ok();
        nbgl_useCaseReviewStatus(STATUS_TYPE_ADDRESS_VERIFIED, ui_idle);
    } else {
        io_seproxyhal_touch_address_cancel();
        nbgl_useCaseReviewStatus(STATUS_TYPE_ADDRESS_REJECTED, ui_idle);
    }
    ui_buffers_cleanup();
}

void ui_display_public_eth2(void) {
    const char *title = "Verify ETH2\naddress";
    uint8_t title_len = 1;  // Initialize lengths to 1 for '\0' character

    // Compute the title message length
    title_len += strlen(title);
    // Initialize the buffers
    if (!ui_buffers_init(title_len, ETH2_ADDRESS_STR, 0)) {
        // Initialization failed, cleanup and return
        io_seproxyhal_send_status(APDU_RESPONSE_INSUFFICIENT_MEMORY, 0, true, true);
        return;
    }
    // Prepare the title message
    strlcpy(g_titleMsg, title, title_len);
    array_bytes_string(g_subTitleMsg, ETH2_ADDRESS_STR, tmpCtx.publicKeyContext.publicKey.W, 48);

    nbgl_useCaseAddressReview(g_subTitleMsg,
                              NULL,
                              get_app_icon(false),
                              g_titleMsg,
                              NULL,
                              reviewChoice);
}
