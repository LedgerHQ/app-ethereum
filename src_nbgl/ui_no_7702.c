#include "nbgl_use_case.h"
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"

static void ui_error_no_7702_choice(bool confirm) {
    if (confirm) {
        ui_settings();
    } else {
        ui_idle();
    }
}

void ui_error_no_7702(void) {
    nbgl_useCaseChoice(&ICON_APP_WARNING,
#ifdef SCREEN_SIZE_WALLET
                       "This authorization cannot be signed",
#else
                       "Unable to sign",
#endif
                       "Enable smart account upgrade in the settings to sign this authorization.",
                       "Go to settings",
                       "Reject authorization",
                       ui_error_no_7702_choice);
}
