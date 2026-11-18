#ifdef HAVE_ETH2

#include "apdu_constants.h"
#include "feature_get_eth2_public_key.h"
#include "common_ui.h"
#include "shared_context.h"

unsigned int io_seproxyhal_touch_eth2_address_ok(void) {
    if (appState != APP_STATE_VERIFYING_ADDRESS) {
        return io_seproxyhal_send_status(SWO_CONDITIONS_NOT_SATISFIED, 0, true, false);
    }
    uint32_t tx = set_result_get_eth2_publicKey();
    return io_seproxyhal_send_status(SWO_SUCCESS, tx, true, false);
}

#endif
