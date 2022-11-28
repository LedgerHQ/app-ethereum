
#include <nbgl_page.h>
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "network.h"
#include "plugins.h"

// 1 more than actually displayed on screen, because of calculations in StaticReview
#define MAX_PLUGIN_ITEMS_PER_SCREEN 4
#define TAG_MAX_LEN 43
#define VALUE_MAX_LEN 79
enum {
  REJECT_TOKEN,
  START_REVIEW_TOKEN,
};

static nbgl_layoutTagValue_t tlv;
// these buffers are used as circular
static char title_buffer[MAX_PLUGIN_ITEMS_PER_SCREEN][TAG_MAX_LEN];
static char msg_buffer[MAX_PLUGIN_ITEMS_PER_SCREEN][VALUE_MAX_LEN];

static nbgl_layoutTagValueList_t useCaseTagValueList;
static nbgl_pageInfoLongPress_t infoLongPress;

struct tx_approval_context_t {
  bool fromPlugin;
  bool blindSigning;
  bool displayNetwork;
};

static struct tx_approval_context_t tx_approval_context;

static void reviewContinueCommon(void);


static void reviewReject(void) {
  io_seproxyhal_touch_tx_cancel(NULL);
  ui_idle();
}

static void confirmTransation(void) {
  io_seproxyhal_touch_tx_ok(NULL);
  ui_idle();
}

static void reviewChoice(bool confirm) {
  if (confirm) {
    confirmTransation();
  } else {
    reviewReject();
  }
}

// called by NBGL to get the tag/value pair corresponding to pairIndex
static nbgl_layoutTagValue_t* getTagValuePair(uint8_t pairIndex) {
  static int counter = 0;

  if (tx_approval_context.fromPlugin) {
    if (pairIndex == 0) {
      // the first page is an exception because the first tag/value pair is the id
      plugin_ui_get_id();
      tlv.item = strings.common.fullAddress;
      tlv.value = strings.common.fullAmount;
    }
    else if (pairIndex <= dataContext.tokenContext.pluginUiMaxItems) {
      // for the next dataContext.tokenContext.pluginUiMaxItems items, get tag/value from plugin_ui_get_item_internal()
      dataContext.tokenContext.pluginUiCurrentItem = pairIndex-1;
      plugin_ui_get_item_internal(
        (uint8_t *)title_buffer[counter],
        TAG_MAX_LEN,
        (uint8_t *)msg_buffer[counter],
        VALUE_MAX_LEN
      );
      tlv.item = title_buffer[counter];
      tlv.value = msg_buffer[counter];
    }
    else {
      // for the last 1 (or 2), tags are fixed
      if (tx_approval_context.displayNetwork && (pairIndex == (dataContext.tokenContext.pluginUiMaxItems+2))) {
        tlv.item = "Network";
        tlv.value = strings.common.network_name;
      }
      else {
        tlv.item = "Max fees";
        tlv.value = strings.common.maxFee;
      }
    }
  } else {
    // if displayNonce is false, we skip index 2
    if ((pairIndex > 1) && (!N_storage.displayNonce)) {
      pairIndex++;
    }

    switch (pairIndex) {
    case 0:
      tlv.item = "Amount";
      tlv.value = strings.common.fullAmount;
      break;
    case 1:
      tlv.item = "Address";
      tlv.value = strings.common.fullAddress;
      break;
    case 2:
      tlv.item = "Nonce";
      tlv.value = strings.common.nonce;
      break;
    case 3:
      tlv.item = "Max fees";
      tlv.value = strings.common.maxFee;
      break;
    case 4:
      tlv.item = "Network";
      tlv.value = strings.common.network_name;
      break;
    }
  }
  // counter is used as index to circular buffer
  counter++;
  if (counter == MAX_PLUGIN_ITEMS_PER_SCREEN) {
    counter = 0;
  }
  return &tlv;
}

static void pageCallback(int token, uint8_t index) {
  (void)index;
  nbgl_pageRelease(pageContext);
  if (token == REJECT_TOKEN) {
    reviewReject();
  }
  else if (token == START_REVIEW_TOKEN) {
    reviewContinueCommon();
  }
}

static void reviewContinue(void) {
  if (tx_approval_context.blindSigning) {
    nbgl_pageInfoDescription_t info = {
      .centeredInfo.icon = &C_icon_warning,
      .centeredInfo.text1 = "Blind Signing",
      .centeredInfo.text2 = NULL,
      .centeredInfo.text3 = NULL,
      .centeredInfo.style = LARGE_CASE_INFO,
      .centeredInfo.offsetY = -32,
      .footerText = "Reject transaction",
      .footerToken = REJECT_TOKEN,
      .tapActionText = "Tap to continue",
      .tapActionToken = START_REVIEW_TOKEN,
      .topRightStyle = NO_BUTTON_STYLE,
      .actionButtonText = NULL,
      .tuneId = TUNE_TAP_CASUAL
    };

    if (pageContext != NULL) {
      nbgl_pageRelease(pageContext);
      pageContext = NULL;
    }
    pageContext = nbgl_pageDrawInfo(&pageCallback, NULL, &info);
  }
  else {
    reviewContinueCommon();
  }
}

static void reviewContinueCommon(void) {
  uint8_t nbPairs = 0;

  if (tx_approval_context.fromPlugin) {
    // plugin id + max items + fees
    nbPairs += 1 + dataContext.tokenContext.pluginUiMaxItems + 1;
    if (tx_approval_context.displayNetwork) {
      nbPairs ++;
    }
  } else {
    nbPairs += 3;
    if (N_storage.displayNonce) {
      nbPairs ++;
    }
    if (tx_approval_context.displayNetwork) {
      nbPairs ++;
    }
  }

  useCaseTagValueList.pairs = NULL;
  useCaseTagValueList.callback = getTagValuePair;
  useCaseTagValueList.startIndex = 0;
  useCaseTagValueList.nbPairs = nbPairs; ///< number of pairs in pairs array
  useCaseTagValueList.smallCaseForValue = false;
  useCaseTagValueList.wrapping = false;
  infoLongPress.icon = &C_badge_transaction_56;
  infoLongPress.text = "Review transaction";
  infoLongPress.longPressText = "Hold to confirm";
  nbgl_useCaseStaticReview(&useCaseTagValueList, &infoLongPress, "Reject", reviewChoice);
}


static void buildFirstPage(void) {
  nbgl_useCaseReviewStart(&C_badge_transaction_56, "Review transaction", NULL, "Reject", reviewContinue, reviewReject);
}

void ux_approve_tx(bool fromPlugin) {
  tx_approval_context.blindSigning = !fromPlugin && tmpContent.txContent.dataPresent && !N_storage.dataAllowed;
  tx_approval_context.fromPlugin = fromPlugin;
  tx_approval_context.displayNetwork = false;

  uint64_t chain_id = get_chain_id();
  if (chainConfig->chainId == ETHEREUM_MAINNET_CHAINID && chain_id != chainConfig->chainId) {
    tx_approval_context.displayNetwork = true;
  }

  buildFirstPage();
}