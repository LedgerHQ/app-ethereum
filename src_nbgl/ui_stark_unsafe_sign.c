
#include "common_ui.h"
#include "ui_nbgl.h"
#include "ui_callbacks.h"
#include "nbgl_use_case.h"

#ifdef HAVE_STARKWARE

static nbgl_layoutTagValue_t tlv[2];
static char from_account[64];
static char message_hash[64];

static void reviewReject(void) {
  io_seproxyhal_touch_tx_cancel(NULL);
  ui_idle();
}

static void confirmTransation(void) {
  io_seproxyhal_touch_stark_unsafe_sign_ok(NULL);
  ui_idle();
}

static void reviewChoice(bool confirm) {
  if (confirm) {
    confirmTransation();
  } else {
    reviewReject();
  }
}

static bool displayTransactionPage(uint8_t page, nbgl_pageContent_t *content) {
  snprintf(from_account, sizeof(from_account), "0x%.*H", 32, dataContext.starkContext.w1);
  snprintf(message_hash, sizeof(message_hash), "0x%.*H", 32, dataContext.starkContext.w2);

  if (page == 0) {
    tlv[0].item = "From Account";
    tlv[0].value = from_account;
    tlv[1].item = "Hash";
    tlv[1].value = message_hash;
    content->type = TAG_VALUE_LIST;
    content->tagValueList.nbPairs = 2;
    content->tagValueList.pairs = (nbgl_layoutTagValue_t *)tlv;
  }
  else if (page == 1) {
    content->type = INFO_LONG_PRESS,
    content->infoLongPress.icon = &ICONGLYPH;
    content->infoLongPress.text = "Unsafe Stark Sign";
    content->infoLongPress.longPressText = "Hold to sign";
  }
  else {
    return false;
  }
  // valid page so return true
  return true;
}

static void reviewContinue(void) {
  nbgl_useCaseRegularReview(0, 2, "Reject", NULL, displayTransactionPage, reviewChoice);
}

static void buildFirstPage(void) {
  nbgl_useCaseReviewStart(&ICONGLYPH,"Unsafe Stark Sign", NULL, "Reject", reviewContinue, reviewReject);
}

void ui_stark_unsafe_sign(void) {
  buildFirstPage();
}

#endif // HAVE_STARKWARE