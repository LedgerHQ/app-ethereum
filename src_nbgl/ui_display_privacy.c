#include "ui_nbgl.h"
#include "ui_callbacks.h"
#include "ui_utils.h"

static void reviewChoice(bool confirm) {
    if (confirm) {
        io_seproxyhal_touch_privacy_ok();
    } else {
        io_seproxyhal_touch_privacy_cancel();
    }
    ui_pairs_cleanup();
}

static void buildFirstPage(const char *review_string) {
    // Initialize the buffers
    if (!ui_pairs_init(2)) {
        // Initialization failed, cleanup and return
        return;
    }

    g_pairs[0].item = "Address";
    g_pairs[0].value = strings.common.toAddress;
    g_pairs[1].item = "Key";
    g_pairs[1].value = strings.common.fullAmount;

    nbgl_useCaseReview(TYPE_OPERATION,
                       g_pairsList,
                       get_tx_icon(false),
                       review_string,
                       NULL,
                       review_string,
                       reviewChoice);
}

void ui_display_privacy_public_key(void) {
    buildFirstPage("Provide public\nprivacy key");
}

void ui_display_privacy_shared_secret(void) {
    buildFirstPage("Provide public\nsecret key");
}
