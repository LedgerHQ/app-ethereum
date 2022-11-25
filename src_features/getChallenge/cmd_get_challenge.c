#ifdef HAVE_TRUSTED_NAME

#include <os.h>
#include <os_io.h>
#include <cx.h>
#include "apdu_constants.h"
#include "challenge.h"
#include "mem.h"
#include "mem_utils.h"

static uint32_t challenge;

void roll_challenge(void) {
    challenge = cx_rng_u32();
}

uint32_t get_challenge(void) {
    return challenge;
}

void handle_get_new_challenge(void) {
    roll_challenge();
    PRINTF("RANDOM DATA -> %u\n", get_challenge());
    U4BE_ENCODE(G_io_apdu_buffer, 0, get_challenge());
    U2BE_ENCODE(G_io_apdu_buffer, 4, APDU_RESPONSE_OK);

    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 6);
}

#endif  // HAVE_TRUSTED_NAME
