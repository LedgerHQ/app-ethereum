#include "common_ui.h"
#include "ui_nbgl.h"
#include "ui_logic.h"
#include "common_712.h"
#include "nbgl_use_case.h"

// 4 pairs of tag/value to display
static nbgl_layoutTagValue_t tlv;

static void reject_message(void) {
  ui_712_reject(NULL);
}

static void sign_message() {
  ui_712_approve(NULL);
}

static void reviewChoice(bool confirm) {
  if (confirm) {
    sign_message();
  } else {
    reject_message();
  }
}
static bool displaySignPage(uint8_t page, nbgl_pageContent_t *content) {
  (void)page;
  content->type = INFO_LONG_PRESS,
  content->infoLongPress.icon = &ICONGLYPH;
  content->infoLongPress.text = "Sign typed message";
  content->infoLongPress.longPressText = "Hold to sign";
  return true;
}

static bool displayTransactionPage(uint8_t page, nbgl_pageContent_t *content) {
  if (page == 0) {
    tlv.item = strings.tmp.tmp2;
    tlv.value = strings.tmp.tmp;
    content->type = TAG_VALUE_LIST;
    content->tagValueList.nbPairs = 1;
    content->tagValueList.pairs = &tlv;
    return true;
  } else {
    switch (ui_712_next_field()) {
        case EIP712_NO_MORE_FIELD:
            return displaySignPage(page, content);
            break;
        case EIP712_FIELD_INCOMING:
        case EIP712_FIELD_LATER:
        default:
            break;
    }
    return false;
  }
}

void ui_712_switch_to_sign(void) {
  nbgl_useCaseRegularReview(0, 0, "Reject", NULL, displaySignPage, reviewChoice);
}

void ui_712_start(void) {
  nbgl_useCaseRegularReview(0, 0, "Reject", NULL, displayTransactionPage, reviewChoice);
}

void ui_712_switch_to_message(void) {
  nbgl_useCaseRegularReview(0, 0, "Reject", NULL, displayTransactionPage, reviewChoice);
}
