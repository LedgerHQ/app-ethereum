#include <string.h>
#include "shared_context.h"

uint32_t set_result_get_publicKey() {
    uint32_t tx = 0;
    G_io_apdu_buffer[tx++] = 65;
    memmove(G_io_apdu_buffer + tx, tmpCtx.publicKeyContext.publicKey.W, 65);
    tx += 65;
    G_io_apdu_buffer[tx++] = 40;
    memmove(G_io_apdu_buffer + tx, tmpCtx.publicKeyContext.address, 40);
    tx += 40;
    if (tmpCtx.publicKeyContext.getChaincode) {
        memmove(G_io_apdu_buffer + tx, tmpCtx.publicKeyContext.chainCode, 32);
        tx += 32;
    }
    return tx;
}
