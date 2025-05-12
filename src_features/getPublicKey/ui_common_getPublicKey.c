#include "apdu_constants.h"
#include "getPublicKey.h"
#include "ui_callbacks.h"

unsigned int io_seproxyhal_touch_address_ok(void) {
    uint32_t tx = set_result_get_publicKey();
    return io_seproxyhal_send_status(APDU_RESPONSE_OK, tx, true, false);
}

unsigned int io_seproxyhal_touch_address_cancel(void) {
    return io_seproxyhal_send_status(APDU_RESPONSE_CONDITION_NOT_SATISFIED, 0, true, false);
}
