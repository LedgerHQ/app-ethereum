#include <nbgl_page.h>
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "ui_signing.h"
#include "starkDisplayUtils.h"
#include "ethUtils.h"
#include "network.h"

#ifdef HAVE_STARKWARE

static nbgl_layoutTagValue_t pairs[3];
static char condAddressBuffer[43];
struct stark_transfer_context {
    bool selfTransfer;
    bool conditional;
};

static struct stark_transfer_context context;

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
    uint8_t count = 0;
    if (page == 0) {
        pairs[count].item = "Amount";
        pairs[count].value = tmpContent.tmp;
        count++;

        if (context.selfTransfer == false && context.conditional == false) {
            pairs[count].item = "Master Account";
            pairs[count].value = strings.tmp.tmp;
            count++;
        }
        if (context.conditional) {
            stark_sign_display_master_account();
            pairs[count].item = "Master Account";
            pairs[count].value = strings.tmp.tmp;
            count++;
        }

        pairs[count].item = "Token Account";
        pairs[count].value = strings.tmp.tmp2;
        count++;

        content->type = TAG_VALUE_LIST;
        content->tagValueList.nbPairs = count;
        content->tagValueList.pairs = (nbgl_layoutTagValue_t *) pairs;

        return true;
    }
    if (page == 1) {
        if (context.conditional) {
            if (!getEthDisplayableAddress(dataContext.starkContext.conditionAddress,
                                          condAddressBuffer,
                                          sizeof(condAddressBuffer),
                                          &global_sha3,
                                          chainConfig->chainId)) {
                return false;
            }
            pairs[0].item = "Cond. Address";
            pairs[0].value = condAddressBuffer;

            stark_sign_display_condition_fact();
            pairs[1].item = "Cond. Address";
            pairs[1].value = strings.tmp.tmp;

            content->type = TAG_VALUE_LIST;
            content->tagValueList.nbPairs = 2;
            content->tagValueList.pairs = (nbgl_layoutTagValue_t *) pairs;

            return true;
        } else {
            page++;
        }
    }
    if (page == 2) {
        content->type = INFO_LONG_PRESS, content->infoLongPress.icon = get_app_icon(false);
        content->infoLongPress.text = "Review transaction";
        content->infoLongPress.longPressText = SIGN_BUTTON;
        return true;
    }

    return false;
}

static void reviewContinue(void) {
    nbgl_useCaseRegularReview(0,
                              context.conditional ? 3 : 2,
                              REJECT_BUTTON,
                              NULL,
                              displayTransactionPage,
                              reviewChoice);
}

void ui_stark_transfer(bool selfTransfer, bool conditional) {
    context.selfTransfer = selfTransfer;
    context.conditional = conditional;
    char *subTitle;
    if (conditional) {
        if (selfTransfer) {
            subTitle = (char *) "Conditional Self Transfer";
        } else {
            subTitle = (char *) "Conditional Transfer";
        }
    } else {
        if (selfTransfer) {
            subTitle = (char *) "Self Transfer";
        } else {
            subTitle = (char *) "Transfer";
        }
    }
    nbgl_useCaseReviewStart(get_app_icon(false),
                            "Review stark\ntransaction",
                            subTitle,
                            REJECT_BUTTON,
                            reviewContinue,
                            reviewReject);
}

#endif  // #ifdef HAVE_STARKWARE
