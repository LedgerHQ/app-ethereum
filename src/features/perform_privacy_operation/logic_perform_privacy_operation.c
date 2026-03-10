#include "shared_context.h"

uint32_t set_result_perform_privacy_operation() {
    for (uint8_t i = 0; i < INT256_LENGTH; i++) {
        G_io_apdu_buffer[i] = tmpCtx.publicKeyContext.publicKey.W[INT256_LENGTH - i];
    }
    return INT256_LENGTH;
}
