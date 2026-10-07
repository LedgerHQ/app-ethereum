#include "ui_nbgl.h"
#include "ui_icons.h"
#include "common_ui.h"
#include "ui_utils.h"
#include "feature_perform_privacy_operation.h"

static void reviewChoice(bool confirm) {
    if (confirm) {
        io_seproxyhal_touch_privacy_ok();
    } else {
        io_seproxyhal_touch_privacy_cancel();
    }
    ui_pairs_cleanup();
}

static void buildFirstPage(const char *review_string, const char *confirm_string) {
    // Initialize the buffers
    if (!ui_pairs_init(2)) {
        // No review will run; scrub the staged secret and release the state
        privacy_operation_cleanup();
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
                       confirm_string,
                       reviewChoice);
}

void ui_display_privacy_public_key(void) {
    buildFirstPage("Review public\nprivacy key", "Provide public\nprivacy key");
}

void ui_display_privacy_shared_secret(void) {
    // The value released here is the X25519 shared secret derived from the
    // device-held private key and the host-supplied peer public key — it is
    // NOT a public value. Wording must make that clear so the user does not
    // approve secret disclosure thinking it is a public-key export.
    buildFirstPage("Review derived\nshared secret", "Provide derived\nshared secret");
}
