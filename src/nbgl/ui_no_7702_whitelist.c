#include "nbgl_use_case.h"
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "i18n/i18n.h"

static void ui_error_no_7702_whitelist_choice(bool confirm) {
    UNUSED(confirm);
    ui_idle();
}

void ui_error_no_7702_whitelist(void) {
    nbgl_useCaseChoice(&ICON_APP_WARNING,
#ifdef SCREEN_SIZE_WALLET
                       STR(AUTH_CANNOT_BE_SIGNED),
#else
                       STR(UNABLE_TO_SIGN),
#endif
                       STR(AUTH_NOT_IN_WHITELIST),
                       STR(BACK_TO_SAFETY),
                       "",
                       ui_error_no_7702_whitelist_choice);
}
