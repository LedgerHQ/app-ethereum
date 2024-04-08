#include "shared_context.h"
#include "apdu_constants.h"

void handleGetAppConfiguration(uint8_t p1,
                               uint8_t p2,
                               const uint8_t *workBuffer,
                               uint8_t dataLength,
                               unsigned int *flags,
                               unsigned int *tx) {
    UNUSED(p1);
    UNUSED(p2);
    UNUSED(workBuffer);
    UNUSED(dataLength);
    UNUSED(flags);
    G_io_apdu_buffer[0] = (N_storage.dataAllowed ? APP_FLAG_DATA_ALLOWED : 0x00);
    G_io_apdu_buffer[0] |= APP_FLAG_EXTERNAL_TOKEN_NEEDED;
    G_io_apdu_buffer[1] = MAJOR_VERSION;
    G_io_apdu_buffer[2] = MINOR_VERSION;
    G_io_apdu_buffer[3] = PATCH_VERSION;
    *tx = 4;
    THROW(0x9000);
}
