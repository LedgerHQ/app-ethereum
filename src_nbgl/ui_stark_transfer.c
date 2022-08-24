#include <nbgl_page.h>
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "starkDisplayUtils.h"
#include "ethUtils.h"

#ifdef HAVE_STARKWARE

static nbgl_layoutTagValue_t tlv[3];
static char condAddressBuffer[43];
struct stark_transfer_context {
  bool selfTransfer;
  bool conditional;
};

static struct stark_transfer_context context;

static void reviewReject(void) {
  io_seproxyhal_touch_tx_cancel(NULL);
  ui_idle();
}

static void confirmTransation(void) {
  io_seproxyhal_touch_stark_ok(NULL);
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
  uint8_t count = 0;
  if (page == 0) {
    tlv[count].item = "Amount";
    tlv[count].value = tmpContent.tmp;
    count++;

    if (context.selfTransfer == false && context.conditional == false) {
      tlv[count].item = "Master Account";
      tlv[count].value = strings.tmp.tmp;
      count++;
    }
    if (context.conditional) {
      stark_sign_display_master_account();
      tlv[count].item = "Master Account";
      tlv[count].value = strings.tmp.tmp;
      count++;
    }
    tlv[count].item = "Token Account";
    tlv[count].value = strings.tmp.tmp2;
    content->type = TAG_VALUE_LIST;
    content->tagValueList.nbPairs = count;
    content->tagValueList.pairs = (nbgl_layoutTagValue_t *)tlv;

    return true;
  }
  if (page == 1) {
    if (context.conditional) {
      getEthDisplayableAddress(dataContext.starkContext.conditionAddress,
                               condAddressBuffer,
                               sizeof(condAddressBuffer),
                               &global_sha3,
                               chainConfig->chainId),
      tlv[0].item = "Cond. Address";
      tlv[0].value = condAddressBuffer;

      stark_sign_display_condition_fact();
      tlv[1].item = "Cond. Address";
      tlv[1].value = strings.tmp.tmp;

      content->type = TAG_VALUE_LIST;
      content->tagValueList.nbPairs = 2;
      content->tagValueList.pairs = (nbgl_layoutTagValue_t *)tlv;

    } else {
      page++;
    }
  }
  if (page == 2) {
    content->type = INFO_LONG_PRESS,
    content->infoLongPress.icon = &C_badge_transaction_56;
    content->infoLongPress.text = "Review transaction";
    content->infoLongPress.longPressText = "Hold to confirm";
  }

  return false;
}

static void reviewContinue(void) {
  nbgl_useCaseRegularReview(0, context.conditional ? 3 : 2, "Reject", NULL, displayTransactionPage, reviewChoice);
}

void ui_stark_transfer(bool selfTransfer, bool conditional) {
  context.selfTransfer = selfTransfer;
  context.conditional  = conditional;
  char* subTitle;
  if (conditional) {
    if (selfTransfer) {
      subTitle = "Conditionnal self transfer";
    } else {
      subTitle = "Conditionnal transfer";
    }
  } else {
    if (selfTransfer) {
      subTitle = "self transfer";
    } else {
      subTitle = "Transfer";
    }
  }
  nbgl_useCaseReviewStart(&C_badge_transaction_56, "Review stark transaction", subTitle, "Reject", reviewContinue, reviewReject);
}

#endif // #ifdef HAVE_STARKWARE
