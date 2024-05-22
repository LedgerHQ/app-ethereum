#include <nbgl_page.h>
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"

static void ui_warning_contract_data_choice(bool confirm) {
    if (confirm) {
        ui_idle();
    } else {
        ui_menu_settings();
    }
}

void ui_warning_contract_data(void) {
    nbgl_useCaseChoice(&C_Warning_64px,
                       "This message cannot\nbe clear-signed",
                       "Enable blind-signing in\nthe settings to sign\nthis transaction.",
                       "Exit",
                       "Go to settings",
                       ui_warning_contract_data_choice);
}
