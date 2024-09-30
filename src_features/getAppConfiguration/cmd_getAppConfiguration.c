#include "shared_context.h"
#include "apdu_constants.h"

uint16_t handleGetAppConfiguration(unsigned int *tx) {
    G_io_apdu_buffer[0] = (N_storage.dataAllowed ? APP_FLAG_DATA_ALLOWED : 0x00);
    G_io_apdu_buffer[0] |= APP_FLAG_EXTERNAL_TOKEN_NEEDED;
    G_io_apdu_buffer[1] = MAJOR_VERSION;
    G_io_apdu_buffer[2] = MINOR_VERSION;
    G_io_apdu_buffer[3] = PATCH_VERSION;
    *tx = 4;
    return APDU_RESPONSE_OK;
}
