#include <nbgl_page.h>
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "uint_common.h"

static void reviewReject(void) {
    io_seproxyhal_touch_address_cancel();
}

static void confirmTransation(void) {
    io_seproxyhal_touch_address_ok();
}

static void reviewChoice(bool confirm) {
    if (confirm) {
        nbgl_useCaseReviewStatus(STATUS_TYPE_ADDRESS_VERIFIED, confirmTransation);
    } else {
        nbgl_useCaseReviewStatus(STATUS_TYPE_ADDRESS_REJECTED, reviewReject);
    }
}

void ui_display_public_eth2(void) {
    array_bytes_string(strings.tmp.tmp,
                       sizeof(strings.tmp.tmp),
                       tmpCtx.publicKeyContext.publicKey.W,
                       48);
    strlcpy(g_stax_shared_buffer, "Verify ETH2\naddress", sizeof(g_stax_shared_buffer));
    nbgl_useCaseAddressReview(strings.tmp.tmp,
                              NULL,
                              get_app_icon(false),
                              g_stax_shared_buffer,
                              NULL,
                              reviewChoice);
}
