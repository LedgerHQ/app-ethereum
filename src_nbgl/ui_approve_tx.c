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
#include "ledger_assert.h"

// 1 more than actually displayed on screen, because of calculations in StaticReview
#define MAX_PLUGIN_ITEMS 8
#define TAG_MAX_LEN      43
#define VALUE_MAX_LEN    79
#define MAX_PAIRS        12  // Max 10 for plugins + 2 (Network and fees)

static nbgl_contentTagValue_t pairs[MAX_PAIRS];
static nbgl_contentTagValueList_t pairsList;
// these buffers are used as circular
static char title_buffer[MAX_PLUGIN_ITEMS][TAG_MAX_LEN];
static char msg_buffer[MAX_PLUGIN_ITEMS][VALUE_MAX_LEN];

struct tx_approval_context_t {
    bool fromPlugin;
    bool blindSigning;
    bool displayNetwork;
#ifdef HAVE_DOMAIN_NAME
    bool domain_name_match;
#endif
};

static struct tx_approval_context_t tx_approval_context;

static void reviewReject(void) {
    io_seproxyhal_touch_tx_cancel(NULL);
    memset(&tx_approval_context, 0, sizeof(tx_approval_context));
}

static void confirmTransation(void) {
    io_seproxyhal_touch_tx_ok(NULL);
    memset(&tx_approval_context, 0, sizeof(tx_approval_context));
}

static void reviewChoice(bool confirm) {
    if (confirm) {
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED, confirmTransation);
    } else {
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_REJECTED, reviewReject);
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

// Force operation to be lowercase
static void get_lowercase_operation(char *dst, size_t dst_len) {
    const char *src = strings.common.fullAmount;
    size_t idx;

    for (idx = 0; (idx < dst_len - 1) && (src[idx] != '\0'); ++idx) {
        dst[idx] = (char) tolower((int) src[idx]);
    }
    dst[idx] = '\0';
}

static uint8_t setTagValuePairs(void) {
    uint8_t nbPairs = 0;
    uint8_t pairIndex = 0;
    uint8_t counter = 0;

    explicit_bzero(pairs, sizeof(pairs));

    // Setup data to display
    if (tx_approval_context.fromPlugin) {
        for (pairIndex = 0; pairIndex < dataContext.tokenContext.pluginUiMaxItems; pairIndex++) {
            // for the next dataContext.tokenContext.pluginUiMaxItems items, get tag/value from
            // plugin_ui_get_item_internal()
            dataContext.tokenContext.pluginUiCurrentItem = pairIndex;
            plugin_ui_get_item_internal((uint8_t *) title_buffer[counter],
                                        TAG_MAX_LEN,
                                        (uint8_t *) msg_buffer[counter],
                                        VALUE_MAX_LEN);
            pairs[nbPairs].item = title_buffer[counter];
            pairs[nbPairs].value = msg_buffer[counter];
            nbPairs++;
            LEDGER_ASSERT((++counter < MAX_PLUGIN_ITEMS), "Too many items for plugin\n");
        }
        // for the last 1 (or 2), tags are fixed
        if (tx_approval_context.displayNetwork) {
            pairs[nbPairs].item = "Network";
            pairs[nbPairs].value = strings.common.network_name;
            nbPairs++;
        }
        pairs[nbPairs].item = "Max fees";
        pairs[nbPairs].value = strings.common.maxFee;
        nbPairs++;
    } else {
        pairs[nbPairs].item = "Amount";
        pairs[nbPairs].value = strings.common.fullAmount;
        nbPairs++;

#ifdef HAVE_DOMAIN_NAME
        uint64_t chain_id = get_tx_chain_id();
        tx_approval_context.domain_name_match =
            has_domain_name(&chain_id, tmpContent.txContent.destination);
        if (tx_approval_context.domain_name_match) {
            pairs[nbPairs].item = "Domain";
            pairs[nbPairs].value = g_domain_name;
            nbPairs++;
        }
        if (!tx_approval_context.domain_name_match || N_storage.verbose_domain_name) {
#endif
            pairs[nbPairs].item = "Address";
            pairs[nbPairs].value = strings.common.fullAddress;
            nbPairs++;
#ifdef HAVE_DOMAIN_NAME
        }
#endif
        if (N_storage.displayNonce) {
            pairs[nbPairs].item = "Nonce";
            pairs[nbPairs].value = strings.common.nonce;
            nbPairs++;
        }
        pairs[nbPairs].item = "Max fees";
        pairs[nbPairs].value = strings.common.maxFee;
        nbPairs++;

        if (tx_approval_context.displayNetwork) {
            pairs[nbPairs].item = "Network";
            pairs[nbPairs].value = strings.common.network_name;
            nbPairs++;
        }
    }
    return nbPairs;
}

static void reviewCommon(void) {
    explicit_bzero(&pairsList, sizeof(pairsList));

    pairsList.nbPairs = setTagValuePairs();
    pairsList.pairs = pairs;

    if (tx_approval_context.fromPlugin) {
        uint32_t buf_size = SHARED_BUFFER_SIZE / 2;
        char op_name[sizeof(strings.common.fullAmount)];
        plugin_ui_get_id();

        get_lowercase_operation(op_name, sizeof(op_name));
        snprintf(g_stax_shared_buffer,
                 buf_size,
                 "Review transaction\nto %s\n%s%s",
                 op_name,
                 (pluginType == EXTERNAL ? "on " : ""),
                 strings.common.fullAddress);
        // Finish text: replace "Review" by "Sign" and add questionmark
        snprintf(g_stax_shared_buffer + buf_size,
                 buf_size,
                 "Sign transaction\nto %s\n%s%s",
                 op_name,
                 (pluginType == EXTERNAL ? "on " : ""),
                 strings.common.fullAddress);

        nbgl_useCaseReview(TYPE_TRANSACTION,
                           &pairsList,
                           get_tx_icon(),
                           g_stax_shared_buffer,
                           NULL,
                           g_stax_shared_buffer + buf_size,
                           reviewChoice);
    } else {
        nbgl_useCaseReview(TYPE_TRANSACTION,
                           &pairsList,
                           get_tx_icon(),
                           REVIEW("transaction"),
                           NULL,
                           SIGN("transaction"),
                           reviewChoice);
    }
}

void blind_confirm_cb(bool confirm) {
    if (confirm) {
        reviewCommon();
    } else {
        reviewReject();
    }
}

void ux_approve_tx(bool fromPlugin) {
    memset(&tx_approval_context, 0, sizeof(tx_approval_context));

    tx_approval_context.blindSigning =
        !fromPlugin && tmpContent.txContent.dataPresent && !N_storage.contractDetails;
    tx_approval_context.fromPlugin = fromPlugin;
    tx_approval_context.displayNetwork = false;

    uint64_t chain_id = get_tx_chain_id();
    if (chainConfig->chainId == ETHEREUM_MAINNET_CHAINID && chain_id != chainConfig->chainId) {
        tx_approval_context.displayNetwork = true;
    }

    if (tx_approval_context.blindSigning) {
        nbgl_useCaseChoice(&C_Important_Circle_64px,
                           "Blind Signing",
                           "This transaction cannot be securely interpreted by "
                           "your Ledger device.\nIt might put "
                           "your assets at risk.",
                           "Continue",
                           "Cancel",
                           blind_confirm_cb);
    } else {
        reviewCommon();
    }
}
