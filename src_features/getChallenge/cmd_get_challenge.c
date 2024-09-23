#ifdef HAVE_TRUSTED_NAME

#include <os.h>
#include <os_io.h>
#include <cx.h>
#include "apdu_constants.h"
#include "challenge.h"

static uint32_t challenge;

/**
 * Generate a new challenge from the Random Number Generator
 */
void roll_challenge(void) {
    challenge = cx_rng_u32();
}

/**
 * Get the current challenge
 *
 * @return challenge
 */
uint32_t get_challenge(void) {
    return challenge;
}

/**
 * Send back the current challenge
 */
uint16_t handle_get_challenge(unsigned int *tx) {
    PRINTF("New challenge -> %u\n", get_challenge());
    U4BE_ENCODE(G_io_apdu_buffer, 0, get_challenge());
    *tx += 4;
    return APDU_RESPONSE_OK;
}

#endif  // HAVE_TRUSTED_NAME
