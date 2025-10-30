#include "ui_nbgl.h"
#include "i18n/i18n.h"

#ifdef SCREEN_SIZE_WALLET
static void ui_error_blind_signing_choice(bool confirm) {
    if (confirm) {
        ui_settings();
    } else {
        ui_idle();
    }
}
#endif

void ui_error_blind_signing(void) {
#ifdef SCREEN_SIZE_WALLET
    nbgl_useCaseChoice(&ICON_APP_WARNING,
                       STR(BLIND_SIGNING_WARNING),
                       STR(BLIND_SIGNING_ERROR),
                       STR(GO_TO_SETTINGS),
                       STR(REJECT_TRANSACTION),
                       ui_error_blind_signing_choice);
#else
    nbgl_useCaseAction(&C_Alert_circle_14px,
                       STR(BLIND_SIGNING_MUST_ENABLE),
                       NULL,
                       ui_idle);
#endif
}
