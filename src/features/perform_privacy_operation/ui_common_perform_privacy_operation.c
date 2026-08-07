#include "apdu_constants.h"
#include "shared_context.h"
#include "common_ui.h"
#include "feature_perform_privacy_operation.h"

// The shared-secret review path renders the X25519 derived secret as hex into
// strings.common.fullAmount, and the corresponding device address into
// strings.common.toAddress. Scrub them on every exit so the secret view does
// not linger in RAM (CWE-312). Surgical instead of a global strings reset:
// other flows reuse the same union for non-sensitive review state.
static void scrub_privacy_strings(void) {
    explicit_bzero(strings.common.fullAmount, sizeof(strings.common.fullAmount));
    explicit_bzero(strings.common.toAddress, sizeof(strings.common.toAddress));
}

unsigned int io_seproxyhal_touch_privacy_ok(void) {
    if (appState != APP_STATE_PERFORMING_PRIVACY_OP) {
        return io_seproxyhal_send_status(SWO_CONDITIONS_NOT_SATISFIED, 0, true, false);
    }
    uint32_t tx = set_result_perform_privacy_operation();
    scrub_privacy_strings();
    return io_seproxyhal_send_status(SWO_SUCCESS, tx, true, true);
}

unsigned int io_seproxyhal_touch_privacy_cancel(void) {
    scrub_privacy_strings();
    return io_seproxyhal_send_status(SWO_CONDITIONS_NOT_SATISFIED, 0, true, true);
}
