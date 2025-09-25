#ifdef HAVE_ETH2

#include "apdu_constants.h"
#include "feature_getEth2PublicKey.h"
#include "ui_callbacks.h"

unsigned int io_seproxyhal_touch_eth2_address_ok(void) {
    uint32_t tx = set_result_get_eth2_publicKey();
    return io_seproxyhal_send_status(APDU_RESPONSE_OK, tx, true, true);
}

#endif
