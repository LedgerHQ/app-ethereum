#include "shared_context.h"
#include "apdu_constants.h"

uint32_t handleGetAppConfiguration(unsigned int *tx) {
    G_io_apdu_buffer[0] = APP_FLAG_EXTERNAL_TOKEN_NEEDED;
    G_io_apdu_buffer[1] = MAJOR_VERSION;
    G_io_apdu_buffer[2] = MINOR_VERSION;
    G_io_apdu_buffer[3] = PATCH_VERSION;
    *tx = 4;
    return APDU_RESPONSE_OK;
}
