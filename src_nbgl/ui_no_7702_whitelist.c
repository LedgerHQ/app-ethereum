#ifdef HAVE_EIP7702_WHITELIST

#include <nbgl_page.h>
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"

static void ui_error_no_7702_whitelist_choice(bool confirm) {
    UNUSED(confirm);
    ui_idle();
}

void ui_error_no_7702_whitelist(void) {
    nbgl_useCaseChoice(&C_Warning_64px,
                       "This 7702 authorisation cannot be signed",
                       "This authorization involves a delegation to a smart contract which is not in the whitelist.",
                       "Back to safety",
                       "",
                       ui_error_no_7702_whitelist_choice);
}

#endif // HAVE_EIP7702_WHITELIST
