#ifdef HAVE_STARKWARE

#include <string.h>
#include "shared_context.h"
#include "feature_stark_getPublicKey.h"

uint32_t set_result_get_stark_publicKey() {
    uint32_t tx = 0;
    memmove(G_io_apdu_buffer + tx, tmpCtx.publicKeyContext.publicKey.W, 65);
    tx += 65;
    return tx;
}

#endif  // HAVE_STARKWARE
