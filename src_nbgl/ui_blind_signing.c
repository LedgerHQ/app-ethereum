#include <nbgl_page.h>
#include "shared_context.h"
#include "ui_callbacks.h"
#include "feature_signTx.h"
#include "ui_nbgl.h"
#include "apdu_constants.h"

static void ui_error_blind_signing_choice(bool confirm) {
    if (confirm) {
        ui_settings();
    } else {
        ui_idle();
    }
}

static void ui_warning_blind_signing_choice(bool confirm) {
    if (confirm) {
        io_seproxyhal_send_status(APDU_RESPONSE_CONDITION_NOT_SATISFIED, 0, true, true);
    } else {
        start_signature_flow();
    }
}

void ui_error_blind_signing(void) {
    nbgl_useCaseChoice(&C_Warning_64px,
                       "This transaction cannot be clear-signed",
                       "Enable blind signing in the settings to sign this transaction.",
                       "Go to settings",
                       "Reject transaction",
                       ui_error_blind_signing_choice);
}

void ui_warning_blind_signing(void) {
    nbgl_useCaseChoice(&C_Warning_64px,
                       "Blind signing ahead",
                       "This transaction's details are not fully verifiable. If you sign it, you "
                       "could lose all your assets.",
                       "Back to safety",
                       "Continue anyway",
                       ui_warning_blind_signing_choice);
}
