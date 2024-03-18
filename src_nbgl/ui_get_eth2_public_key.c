#include <nbgl_page.h>
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"

static void reviewReject(void) {
    io_seproxyhal_touch_address_cancel(NULL);
}

static void confirmTransation(void) {
    io_seproxyhal_touch_address_ok(NULL);
}

static void reviewChoice(bool confirm) {
    if (confirm) {
        // display a status page and go back to main
        nbgl_useCaseStatus("ADDRESS\nVERIFIED", true, confirmTransation);
    } else {
        nbgl_useCaseStatus("Address verification\ncancelled", false, reviewReject);
    }
}

static void buildScreen(void) {
    snprintf(strings.tmp.tmp, 100, "0x%.*H", 48, tmpCtx.publicKeyContext.publicKey.W);
    nbgl_useCaseAddressConfirmation(strings.tmp.tmp, reviewChoice);
}

void ui_display_public_eth2(void) {
    buildScreen();
}
