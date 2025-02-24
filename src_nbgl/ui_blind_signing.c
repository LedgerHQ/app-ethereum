#include "ui_nbgl.h"

static void ui_error_blind_signing_choice(bool confirm) {
    if (confirm) {
        ui_settings();
    } else {
        ui_idle();
    }
}

void ui_error_blind_signing(void) {
    nbgl_useCaseChoice(&ICON_APP_WARNING,
                       "This transaction cannot be clear-signed",
                       "Enable blind signing in the settings to sign this transaction.",
                       "Go to settings",
                       "Reject transaction",
                       ui_error_blind_signing_choice);
}
