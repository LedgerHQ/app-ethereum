/*
 * Fuzz APDU dispatcher (thin wrapper).
 *
 * Hands the parsed command to production `handleApdu()`
 * (`src/apdu_dispatch.c`). Adding a new INS in production is
 * immediately reachable from the fuzzer with no harness-side edit.
 *
 * Status word and response length are discarded; only side effects on
 * app globals and sanitizer-observable memory matter here.
 */
#include "fuzz_apdu_dispatch.h"

#include "apdu_dispatch.h"

void fuzz_dispatch_apdu(command_t *cmd) {
    uint32_t tx = 0;

    (void) handleApdu(cmd, &tx);
}
