#include "shared_context.h"
#include "apdu_constants.h"

#include "ui_flow.h"

void handleGetAppConfiguration(uint8_t p1,
                               uint8_t p2,
                               uint8_t *workBuffer,
                               uint16_t dataLength,
                               unsigned int *flags,
                               unsigned int *tx) {
    UNUSED(p1);
    UNUSED(p2);
    UNUSED(workBuffer);
    UNUSED(dataLength);
    UNUSED(flags);
    G_io_apdu_buffer[0] = (N_storage.dataAllowed ? APP_FLAG_DATA_ALLOWED : 0x00);
#ifndef HAVE_TOKENS_LIST
    G_io_apdu_buffer[0] |= APP_FLAG_EXTERNAL_TOKEN_NEEDED;
#endif
#ifdef HAVE_STARKWARE
    G_io_apdu_buffer[0] |= APP_FLAG_STARKWARE;
    G_io_apdu_buffer[0] |= APP_FLAG_STARKWARE_V2;
#endif
    G_io_apdu_buffer[1] = LEDGER_MAJOR_VERSION;
    G_io_apdu_buffer[2] = LEDGER_MINOR_VERSION;
    G_io_apdu_buffer[3] = LEDGER_PATCH_VERSION;
    *tx = 4;
    THROW(0x9000);
}
