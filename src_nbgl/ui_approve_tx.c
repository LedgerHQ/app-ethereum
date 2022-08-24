
#include <nbgl_page.h>
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "network.h"
#include "plugins.h"

#define MAX_PLUGIN_ITEMS_PER_SCREEN 3

static nbgl_layoutTagValue_t tlv[MAX_PLUGIN_ITEMS_PER_SCREEN];
static char title_buffer[MAX_PLUGIN_ITEMS_PER_SCREEN][43];
static char msg_buffer[MAX_PLUGIN_ITEMS_PER_SCREEN][79];

static uint8_t pageCount = 0;
struct tx_approval_context_t {
  bool fromPlugin;
  bool blindSigning;
  bool displayNetwork;
};

static struct tx_approval_context_t tx_approval_context;


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

static void buildBlindSignWarningPage(nbgl_pageContent_t *content) {
  content->type = CENTERED_INFO;
  content->centeredInfo.icon = &C_icon_warning;
  content->centeredInfo.text1 = "Blind Signing";
  content->centeredInfo.style = LARGE_CASE_INFO;
}

static void buildRegularTransactionInfoAmountPage(nbgl_pageContent_t *content) {
  tlv[0].item = "Amount";
  tlv[1].item = "Address";
  tlv[2].item = "Nonce";

  tlv[0].value = strings.common.fullAmount;
  tlv[1].value = strings.common.fullAddress;
  tlv[2].value = strings.common.nonce;

  content->type = TAG_VALUE_LIST;
  content->tagValueList.nbPairs = N_storage.displayNonce ? 3 : 2;
  content->tagValueList.pairs = (nbgl_layoutTagValue_t *)tlv;
}

static void buildRegularTransactionInfoAddressPage(nbgl_pageContent_t *content) {
  tlv[0].item = "Max fees";
  tlv[1].item = "Network";

  tlv[0].value = strings.common.maxFee;
  tlv[1].value = strings.common.network_name;

  content->type = TAG_VALUE_LIST;
  content->tagValueList.nbPairs = tx_approval_context.displayNetwork ? 2 : 1;
  content->tagValueList.pairs = (nbgl_layoutTagValue_t *)tlv;
}

static void buildRegularTransactionInfoConfirm(nbgl_pageContent_t *content) {
  content->type = INFO_LONG_PRESS,
  content->infoLongPress.icon = &C_badge_transaction_56;
  content->infoLongPress.text = "Review transaction";
  content->infoLongPress.longPressText = "Hold to confirm";
}

static bool displayTransactionPage(uint8_t page, nbgl_pageContent_t *content) {
  if (tx_approval_context.blindSigning) {
    if (page == 0) {
      buildBlindSignWarningPage(content);
      return true;
    }
  } else {
    page++;
  }

  if (tx_approval_context.fromPlugin) {
    if (page == 1) {
      plugin_ui_get_id();
      tlv[0].item = strings.common.fullAddress;
      tlv[0].value = strings.common.fullAmount;

      content->type = TAG_VALUE_LIST;
      content->tagValueList.nbPairs = 1;
      content->tagValueList.pairs = (nbgl_layoutTagValue_t *)tlv;

      return true;
    }
    else if (page < pageCount - 1) {
      uint8_t count = 0;
      for (
        dataContext.tokenContext.pluginUiCurrentItem = (page - 2) * MAX_PLUGIN_ITEMS_PER_SCREEN;
        dataContext.tokenContext.pluginUiCurrentItem < (page - 1) * MAX_PLUGIN_ITEMS_PER_SCREEN && dataContext.tokenContext.pluginUiCurrentItem < dataContext.tokenContext.pluginUiMaxItems;
        dataContext.tokenContext.pluginUiCurrentItem++, count++
        ) {
        plugin_ui_get_item_internal(
          title_buffer[count],
          sizeof(title_buffer[0]),
          msg_buffer[count],
          sizeof(msg_buffer[0])
        );
        tlv[count].item = title_buffer[count];
        tlv[count].value = msg_buffer[count];
      }
      content->type = TAG_VALUE_LIST;
      content->tagValueList.pairs = (nbgl_layoutTagValue_t *)tlv;
      content->tagValueList.nbPairs = count;

      return true;
    }
    else if (page == pageCount - 1) {
      buildRegularTransactionInfoAddressPage(content);
      return true;
    }
    else
    {
      buildRegularTransactionInfoConfirm(content);
      return true;
    }
  } else {
    if (page == 1) {
      buildRegularTransactionInfoAmountPage(content);
      return true;
    }
    if (page == 2) {
      buildRegularTransactionInfoAddressPage(content);
      return true;
    }
    if (page == 3) {
      buildRegularTransactionInfoConfirm(content);
      return true;
    }
  }
  return false;
}

static void reviewContinue(void) {
  pageCount = 0;

  if (tx_approval_context.blindSigning) {
    pageCount++;
  }

  if (tx_approval_context.fromPlugin) {
    // plugin id + max items + fees + confirm
    pageCount += 1 + (dataContext.tokenContext.pluginUiMaxItems/MAX_PLUGIN_ITEMS_PER_SCREEN) + 1 + 1;
    if (dataContext.tokenContext.pluginUiMaxItems%MAX_PLUGIN_ITEMS_PER_SCREEN) {
      pageCount++;
    }
  } else {
    pageCount += 3;
  }
  nbgl_useCaseRegularReview(0, pageCount, "Reject", NULL, displayTransactionPage, reviewChoice);
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