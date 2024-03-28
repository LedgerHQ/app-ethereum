#include <ctype.h>
#include <nbgl_page.h>
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "ui_signing.h"
#include "plugins.h"
#include "domain_name.h"
#include "caller_api.h"
#include "network_icons.h"
#include "network.h"

#define TEXT_TX "transaction"
// 1 more than actually displayed on screen, because of calculations in StaticReview
#define MAX_PLUGIN_ITEMS_PER_SCREEN 4
#define TAG_MAX_LEN                 43
#define VALUE_MAX_LEN               79
enum {
    REJECT_TOKEN,
    START_REVIEW_TOKEN,
};

static nbgl_layoutTagValue_t pair;
// these buffers are used as circular
static char title_buffer[MAX_PLUGIN_ITEMS_PER_SCREEN][TAG_MAX_LEN];
static char msg_buffer[MAX_PLUGIN_ITEMS_PER_SCREEN][VALUE_MAX_LEN];
static nbgl_layoutTagValueList_t useCaseTagValueList;
static nbgl_pageInfoLongPress_t infoLongPress;

struct tx_approval_context_t {
    bool fromPlugin;
    bool blindSigning;
    bool displayNetwork;
#ifdef HAVE_DOMAIN_NAME
    bool domain_name_match;
#endif
};

static struct tx_approval_context_t tx_approval_context;

static void reviewContinueCommon(void);

static void reviewReject(void) {
    io_seproxyhal_touch_tx_cancel(NULL);
    memset(&tx_approval_context, 0, sizeof(tx_approval_context));
}

static void confirmTransation(void) {
    io_seproxyhal_touch_tx_ok(NULL);
    memset(&tx_approval_context, 0, sizeof(tx_approval_context));
}

static void onConfirmAbandon(void) {
    nbgl_useCaseStatus("Transaction rejected", false, reviewReject);
}

static void rejectTransactionQuestion(void) {
    nbgl_useCaseConfirm(REJECT_QUESTION(TEXT_TX),
                        NULL,
                        REJECT_CONFIRM_BUTTON,
                        RESUME(TEXT_TX),
                        onConfirmAbandon);
}

static void reviewChoice(bool confirm) {
    if (confirm) {
        nbgl_useCaseStatus("TRANSACTION\nSIGNED", true, confirmTransation);
    } else {
        rejectTransactionQuestion();
    }
}

// called by NBGL to get the tag/value pair corresponding to pairIndex
static nbgl_layoutTagValue_t *getTagValuePair(uint8_t pairIndex) {
    static int counter = 0;

    if (tx_approval_context.fromPlugin) {
        if (pairIndex < dataContext.tokenContext.pluginUiMaxItems) {
            // for the next dataContext.tokenContext.pluginUiMaxItems items, get tag/value from
            // plugin_ui_get_item_internal()
            dataContext.tokenContext.pluginUiCurrentItem = pairIndex;
            plugin_ui_get_item_internal((uint8_t *) title_buffer[counter],
                                        TAG_MAX_LEN,
                                        (uint8_t *) msg_buffer[counter],
                                        VALUE_MAX_LEN);
            pair.item = title_buffer[counter];
            pair.value = msg_buffer[counter];
        } else {
            pairIndex -= dataContext.tokenContext.pluginUiMaxItems;
            // for the last 1 (or 2), tags are fixed
            if (tx_approval_context.displayNetwork && (pairIndex == 0)) {
                pair.item = "Network";
                pair.value = strings.common.network_name;
            } else {
                pair.item = "Max fees";
                pair.value = strings.common.maxFee;
            }
        }
    } else {
        uint8_t target_index = 0;

        if (pairIndex == target_index++) {
            pair.item = "Amount";
            pair.value = strings.common.fullAmount;
        }
        if ((strings.common.fromAddress[0] != 0) && (pairIndex == target_index++)) {
            pair.item = "Address From";
            pair.value = strings.common.fromAddress;
        }

#ifdef HAVE_DOMAIN_NAME
        if (tx_approval_context.domain_name_match) {
            if (pairIndex == target_index++) {
                pair.item = "Domain";
                pair.value = g_domain_name;
            }
        }
        if (!tx_approval_context.domain_name_match || N_storage.verbose_domain_name) {
#endif  // HAVE_DOMAIN_NAME
            if (pairIndex == target_index++) {
                pair.item = "Address To";
                pair.value = strings.common.fullAddress;
            }

#ifdef HAVE_DOMAIN_NAME
        }
#endif  // HAVE_DOMAIN_NAME
        if (N_storage.displayNonce) {
            if (pairIndex == target_index++) {
                pair.item = "Nonce";
                pair.value = strings.common.nonce;
            }
        }
        if (pairIndex == target_index++) {
            pair.item = "Max fees";
            pair.value = strings.common.maxFee;
        }
        if (pairIndex == target_index++) {
            pair.item = "Network";
            pair.value = strings.common.network_name;
        }
    }
    // counter is used as index to circular buffer
    counter++;
    if (counter == MAX_PLUGIN_ITEMS_PER_SCREEN) {
        counter = 0;
    }
    return &pair;
}

static void pageCallback(int token, uint8_t index) {
    (void) index;
    nbgl_pageRelease(pageContext);
    if (token == REJECT_TOKEN) {
        reviewReject();
    } else if (token == START_REVIEW_TOKEN) {
        reviewContinueCommon();
    }
}

static void reviewContinue(void) {
    if (tx_approval_context.blindSigning) {
        nbgl_pageInfoDescription_t info = {
            .centeredInfo.icon = &C_round_warning_64px,
            .centeredInfo.text1 = "Blind Signing",
            .centeredInfo.text2 =
                "This transaction cannot be\nsecurely interpreted by Ledger\nStax. It might put "
                "your assets\nat risk.",
            .centeredInfo.text3 = NULL,
            .centeredInfo.style = LARGE_CASE_INFO,
            .centeredInfo.offsetY = -32,
            .footerText = REJECT(TEXT_TX),
            .footerToken = REJECT_TOKEN,
            .tapActionText = "Tap to continue",
            .tapActionToken = START_REVIEW_TOKEN,
            .topRightStyle = NO_BUTTON_STYLE,
            .actionButtonText = NULL,
            .tuneId = TUNE_TAP_CASUAL};

        if (pageContext != NULL) {
            nbgl_pageRelease(pageContext);
            pageContext = NULL;
        }
        pageContext = nbgl_pageDrawInfo(&pageCallback, NULL, &info);
    } else {
        reviewContinueCommon();
    }
}

static const nbgl_icon_details_t *get_tx_icon(void) {
    const nbgl_icon_details_t *icon = NULL;

    if (tx_approval_context.fromPlugin && (pluginType == EXTERNAL)) {
        if (caller_app && caller_app->name) {
            if ((strlen(strings.common.fullAddress) == strlen(caller_app->name)) &&
                (strcmp(strings.common.fullAddress, caller_app->name) == 0)) {
                icon = get_app_icon(true);
            }
        }
    } else {
        uint64_t chain_id = get_tx_chain_id();
        if (chain_id == chainConfig->chainId) {
            icon = get_app_icon(false);
        } else {
            icon = get_network_icon_from_chain_id(&chain_id);
        }
    }
    return icon;
}

static void reviewContinueCommon(void) {
    uint8_t nbPairs = 0;

    if (tx_approval_context.fromPlugin) {
        // plugin id + max items + fees
        nbPairs += dataContext.tokenContext.pluginUiMaxItems + 1;
    } else {
        nbPairs += 4;
        if (N_storage.displayNonce) {
            nbPairs++;
        }
#ifdef HAVE_DOMAIN_NAME
        uint64_t chain_id = get_tx_chain_id();
        tx_approval_context.domain_name_match =
            has_domain_name(&chain_id, tmpContent.txContent.destination);
        if (tx_approval_context.domain_name_match && N_storage.verbose_domain_name) {
            nbPairs += 1;
        }
#endif  // HAVE_DOMAIN_NAME
    }

    if (tx_approval_context.displayNetwork) {
        nbPairs++;
    }

    useCaseTagValueList.pairs = NULL;
    useCaseTagValueList.callback = getTagValuePair;
    useCaseTagValueList.startIndex = 0;
    useCaseTagValueList.nbPairs = nbPairs;  ///< number of pairs in pairs array
    useCaseTagValueList.smallCaseForValue = false;
    useCaseTagValueList.wrapping = false;
    infoLongPress.icon = get_tx_icon();
    infoLongPress.text = tx_approval_context.fromPlugin ? g_stax_shared_buffer : SIGN(TEXT_TX);
    infoLongPress.longPressText = SIGN_BUTTON;
    nbgl_useCaseStaticReview(&useCaseTagValueList, &infoLongPress, REJECT(TEXT_TX), reviewChoice);
}

// Replace "Review" by "Sign" and add questionmark
static void prepare_sign_text(void) {
    uint8_t sign_length = strlen("Sign");
    uint8_t review_length = strlen("Review");

    memmove(g_stax_shared_buffer, "Sign", sign_length);
    memmove(g_stax_shared_buffer + sign_length,
            g_stax_shared_buffer + review_length,
            strlen(g_stax_shared_buffer) - review_length + 1);
    strlcat(g_stax_shared_buffer, "?", sizeof(g_stax_shared_buffer));
}

// Force operation to be lowercase
static void get_lowercase_operation(char *dst, size_t dst_len) {
    const char *src = strings.common.fullAmount;
    size_t idx;

    for (idx = 0; (idx < dst_len - 1) && (src[idx] != '\0'); ++idx) {
        dst[idx] = (char) tolower((int) src[idx]);
    }
    dst[idx] = '\0';
}

static void buildFirstPage(void) {
    if (tx_approval_context.fromPlugin) {
        char op_name[sizeof(strings.common.fullAmount)];
        plugin_ui_get_id();

        get_lowercase_operation(op_name, sizeof(op_name));
        if (pluginType == EXTERNAL) {
            snprintf(g_stax_shared_buffer,
                     sizeof(g_stax_shared_buffer),
                     "Review transaction\nto %s\non %s",
                     op_name,
                     strings.common.fullAddress);
        } else {
            snprintf(g_stax_shared_buffer,
                     sizeof(g_stax_shared_buffer),
                     "Review transaction\nto %s\n%s",
                     op_name,
                     strings.common.fullAddress);
        }
        nbgl_useCaseReviewStart(get_tx_icon(),
                                g_stax_shared_buffer,
                                NULL,
                                REJECT(TEXT_TX),
                                reviewContinue,
                                rejectTransactionQuestion);
        prepare_sign_text();
    } else {
        nbgl_useCaseReviewStart(get_tx_icon(),
                                REVIEW(TEXT_TX),
                                NULL,
                                REJECT(TEXT_TX),
                                reviewContinue,
                                rejectTransactionQuestion);
    }
}

void ux_approve_tx(bool fromPlugin) {
    tx_approval_context.blindSigning =
        !fromPlugin && tmpContent.txContent.dataPresent && !N_storage.contractDetails;
    tx_approval_context.fromPlugin = fromPlugin;
    tx_approval_context.displayNetwork = false;

    uint64_t chain_id = get_tx_chain_id();
    if (chainConfig->chainId == ETHEREUM_MAINNET_CHAINID && chain_id != chainConfig->chainId) {
        tx_approval_context.displayNetwork = true;
    }

    buildFirstPage();
}
