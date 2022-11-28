#include "common_ui.h"
#include "ui_nbgl.h"

static nbgl_layoutTagValue_t tlv;

static void reviewReject(void) {
  io_seproxyhal_touch_data_cancel(NULL);
  ui_idle();
}

static void confirmTransation(void) {
  io_seproxyhal_touch_data_ok(NULL);
}

static void reviewChoice(bool confirm) {
  if (confirm) {
    confirmTransation();
  } else {
    reviewReject();
  }
}

static bool displayTransactionPage(uint8_t page, nbgl_pageContent_t *content) {
  if (page == 0) {
    tlv.item = "Parameter";
    tlv.value = strings.tmp.tmp;
    content->type = TAG_VALUE_LIST;
    content->tagValueList.nbPairs = 1;
    content->tagValueList.pairs = (nbgl_layoutTagValue_t *)&tlv;
  }
  else if (page == 1) {
    content->type = INFO_LONG_PRESS,
    content->infoLongPress.icon = &ICONGLYPH;
    content->infoLongPress.text = "Confirm parameter";
    content->infoLongPress.longPressText = "Hold to confirm";
  }
  else {
    return false;
  }
  // valid page so return true
  return true;
}


static void reviewContinue(void) {
  nbgl_useCaseRegularReview(0, 2, "Reject parameter", NULL, displayTransactionPage, reviewChoice);
}

static void buildScreen(void) {
  nbgl_useCaseReviewStart(&ICONGLYPH, "Verify parameter", NULL, "Reject", reviewContinue, reviewReject);
}

void ui_confirm_parameter(void) {
  buildScreen();
}
