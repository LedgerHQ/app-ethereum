#include <nbgl_page.h>
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"

static void reviewReject(void) {
  io_seproxyhal_touch_address_cancel(NULL);
  ui_idle();
}

static void confirmTransation(void) {
  io_seproxyhal_touch_stark_pubkey_ok(NULL);
  ui_idle();
}

static void reviewChoice(bool confirm) {
  if (confirm) {
    confirmTransation();
  } else {
    reviewReject();
  }
}

static void buildScreen(void) {
  nbgl_useCaseChoice(
    &C_warning64px,
    "Verify stark key",
    strings.tmp.tmp,
    "Approuve",
    "Reject",
    reviewChoice
  );
}

void ui_display_stark_public(void) {
  buildScreen();
}