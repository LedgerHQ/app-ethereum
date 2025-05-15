#include "nbgl_use_case.h"
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"

static void ui_error_no_7702_whitelist_choice(bool confirm) {
    UNUSED(confirm);
    ui_idle();
}

void ui_error_no_7702_whitelist(void) {
    nbgl_useCaseChoice(&ICON_APP_WARNING,
#ifdef SCREEN_SIZE_WALLET
                       "This authorization cannot be signed",
#else
                       "Unable to sign",
#endif
                       "This authorization involves a delegation to a smart contract which is not "
                       "in the whitelist.",
                       "Back to safety",
                       "",
                       ui_error_no_7702_whitelist_choice);
}
