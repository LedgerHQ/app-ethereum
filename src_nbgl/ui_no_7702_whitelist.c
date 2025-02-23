#ifdef HAVE_EIP7702_WHITELIST

#include <nbgl_page.h>
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"

static void ui_error_no_7702_whitelist_choice(bool confirm) {
    if (confirm) {
        ui_settings();
    } else {
        ui_idle();
    }
}

void ui_error_no_7702_whitelist(void) {
    nbgl_useCaseChoice(&C_Warning_64px,
                       "This 7702 authorisation cannot be signed",
                       "Enable No 7702 Whitelist in the settings to sign this transaction.",
                       "Go to settings",
                       "Reject transaction",
                       ui_error_no_7702_whitelist_choice);
}

#endif // HAVE_EIP7702_WHITELIST
