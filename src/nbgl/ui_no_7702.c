#include "nbgl_use_case.h"
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "i18n/i18n.h"

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
                       STR(AUTH_CANNOT_BE_SIGNED),
#else
                       STR(UNABLE_TO_SIGN),
#endif
                       STR(ENABLE_SMART_ACCOUNT_SETTING),
                       STR(GO_TO_SETTINGS),
                       STR(REJECT_AUTHORIZATION),
                       ui_error_no_7702_choice);
}
