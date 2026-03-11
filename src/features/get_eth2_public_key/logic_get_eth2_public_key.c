#ifdef HAVE_ETH2

#include <string.h>
#include "shared_context.h"

uint32_t set_result_get_eth2_publicKey() {
    uint32_t tx = 0;
    memmove(G_io_tx_buffer + tx,
            tmpCtx.publicKeyContext.publicKey.W,
            BLS12381_G1_COMPRESSED_PUBKEY_LENGTH);
    tx += BLS12381_G1_COMPRESSED_PUBKEY_LENGTH;
    return tx;
}

#endif  // HAVE_ETH2
