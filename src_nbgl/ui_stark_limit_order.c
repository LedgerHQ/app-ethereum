#include "common_ui.h"
#include "ui_nbgl.h"
#include "ui_callbacks.h"
#include "nbgl_use_case.h"
#include "network.h"


#ifdef HAVE_STARKWARE

static nbgl_layoutTagValue_t tlv[3];

static void reviewReject(void) {
  io_seproxyhal_touch_tx_cancel(NULL);
}

static void confirmTransation(void) {
  io_seproxyhal_touch_stark_ok(NULL);
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
    tlv[0].item = "Sell";
    tlv[0].value = strings.common.fullAmount;
    tlv[1].item = "Buy";
    tlv[1].value = strings.common.maxFee;
    tlv[2].item = "Token amount";
    tlv[2].value = strings.common.fullAddress;

    content->type = TAG_VALUE_LIST;
    content->tagValueList.nbPairs = 3;
    content->tagValueList.pairs = (nbgl_layoutTagValue_t *)tlv;
  }
  else if (page == 1) {
    content->type = INFO_LONG_PRESS,
    content->infoLongPress.icon = get_app_chain_icon();
    content->infoLongPress.text = "Review stark limit order";
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
  nbgl_useCaseReviewStart(get_app_chain_icon(), "Review stark limit order", NULL, "Reject", reviewContinue, reviewReject);
}

void ui_stark_limit_order(void) {
  buildFirstPage();
}

#endif