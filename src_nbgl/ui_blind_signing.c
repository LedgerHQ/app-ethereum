#include <nbgl_page.h>
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "apdu_constants.h"
#include "context_712.h"

static void ui_error_blind_signing_choice(bool confirm) {
    if (confirm) {
        ui_settings();
    } else {
        ui_idle();
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
