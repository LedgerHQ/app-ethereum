#include "apdu_constants.h"
#include "ui_callbacks.h"
#include "feature_performPrivacyOperation.h"

unsigned int io_seproxyhal_touch_privacy_ok(void) {
    uint32_t tx = set_result_perform_privacy_operation();
    return io_seproxyhal_send_status(SWO_SUCCESS, tx, true, true);
}

unsigned int io_seproxyhal_touch_privacy_cancel(void) {
    return io_seproxyhal_send_status(SWO_CONDITIONS_NOT_SATISFIED, 0, true, true);
}
