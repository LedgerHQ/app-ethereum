#include "shared_context.h"

uint32_t set_result_perform_privacy_operation() {
    for (uint8_t i = 0; i < 32; i++) {
        G_io_apdu_buffer[i] = tmpCtx.publicKeyContext.publicKey.W[32 - i];
    }
    return 32;
}
