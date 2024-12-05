#include "shared_context.h"
#include "apdu_constants.h"
#include "feature_getPublicKey.h"
#include "common_ui.h"
#include "feature_performPrivacyOperation.h"

unsigned int io_seproxyhal_touch_privacy_ok(void) {
    uint32_t tx = set_result_perform_privacy_operation();
    return io_seproxyhal_send_status(APDU_RESPONSE_OK, tx, true, true);
}

unsigned int io_seproxyhal_touch_privacy_cancel(void) {
    return io_seproxyhal_send_status(APDU_RESPONSE_CONDITION_NOT_SATISFIED, 0, true, true);
}
