#include "mocks.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "bip32_utils.h"
#include "bip32.h"
#include "cx_errors.h"
#include "lcx_ecfp.h"
#include "tlv_library.h"
#include "exceptions.h"
#include "os_task.h"
#include "main_std_app.h"  // app_exit (WEAK in excluded lib_standard_app/main.c)

/* ── Stubs for symbols from excluded main.c ─────────────────────────── */

uint16_t io_seproxyhal_send_status(uint16_t sw, uint32_t tx, bool reset, bool idle) {
    (void) sw;
    (void) tx;
    (void) reset;
    (void) idle;
    return 0;
}

void app_main(void) {
}

void coin_main(void *args) {
    (void) args;
}
void library_main(void *args) {
    (void) args;
}
void clone_main(void *args) {
    (void) args;
}
int ethereum_main(void *args) {
    (void) args;
    return 0;
}

/* ── Mock data for ui_icons.c ────────────────────────────────────────── */
/* net_icons.gen.c (the tools/gen_networks.py output) is not built in the fuzz
 * tree, so provide the g_network_icons array it would define. ui_icons.c
 * iterates it for the hardcoded-network icon fallback. Wallet screens only;
 * Nano builds have no such array. */
#ifdef SCREEN_SIZE_WALLET
#include "net_icons.gen.h"
const network_icon_t g_network_icons[10] = {0};
#endif
