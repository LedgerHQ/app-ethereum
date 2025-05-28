#include <ctype.h>
#include "os_utils.h"
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_message_signing.h"
#include "ui_nbgl.h"
#include "plugins.h"
#include "trusted_name.h"
#include "caller_api.h"
#include "network_icons.h"
#include "network.h"
#include "cmd_get_tx_simulation.h"
#include "utils.h"

// 1 more than actually displayed on screen, because of calculations in StaticReview
#define MAX_PLUGIN_ITEMS 8
#define TAG_MAX_LEN      43
#define VALUE_MAX_LEN    79
#define MAX_PAIRS        12  // Max 10 for plugins + 2 (Network and fees)

static nbgl_contentTagValue_t pairs[MAX_PAIRS];
static nbgl_contentTagValueList_t pairsList;
static nbgl_contentValueExt_t extension = {0};
// these buffers are used as circular
static char title_buffer[MAX_PLUGIN_ITEMS][TAG_MAX_LEN];
static char msg_buffer[MAX_PLUGIN_ITEMS][VALUE_MAX_LEN];

struct tx_approval_context_t {
    bool fromPlugin;
    bool displayNetwork;
    bool trusted_name_match;
};

static struct tx_approval_context_t tx_approval_context;

static void reviewChoice(bool confirm) {
    if (confirm) {
        io_seproxyhal_touch_tx_ok();
        memset(&tx_approval_context, 0, sizeof(tx_approval_context));
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED, ui_idle);
    } else {
        io_seproxyhal_touch_tx_cancel();
        memset(&tx_approval_context, 0, sizeof(tx_approval_context));
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_REJECTED, ui_idle);
    }
#ifdef HAVE_WEB3_CHECKS
    clear_tx_simulation();
#endif
}

const nbgl_icon_details_t *get_tx_icon(void) {
    const nbgl_icon_details_t *icon = NULL;

    if (tx_approval_context.fromPlugin && (pluginType == EXTERNAL)) {
        if ((caller_app != NULL) && (caller_app->name != NULL)) {
            if (strcmp(strings.common.toAddress, caller_app->name) == 0) {
                icon = get_app_icon(true);
            }
        }
        // icon is NULL in this case
        // Check with Alex if this is expected or a bug
    } else if ((caller_app != NULL) && !tx_approval_context.fromPlugin) {
        // Clone case
        icon = get_app_icon(true);
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
        if (pluginType != EXTERNAL) {
            if (strings.common.fromAddress[0] != 0) {
                pairs[nbPairs].item = "From";
                pairs[nbPairs].value = strings.common.fromAddress;
                nbPairs++;
            }
        }
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
        // Display the From address
        // ------------------------
        if (strings.common.fromAddress[0] != 0) {
            pairs[nbPairs].item = "From";
            pairs[nbPairs].value = strings.common.fromAddress;
            nbPairs++;
        }

        // Display the Amount
        // ------------------
        if (!tmpContent.txContent.dataPresent ||
            !allzeroes(tmpContent.txContent.value.value, tmpContent.txContent.value.length)) {
            pairs[nbPairs].item = "Amount";
            pairs[nbPairs].value = strings.common.fullAmount;
            nbPairs++;
        }

        // Display the To address
        // ----------------------
        uint64_t chain_id = get_tx_chain_id();
        e_name_type type = TN_TYPE_ACCOUNT;
        e_name_source source = TN_SOURCE_ENS;
        tx_approval_context.trusted_name_match =
            get_trusted_name(1, &type, 1, &source, &chain_id, tmpContent.txContent.destination);
        pairs[nbPairs].item = "To";
        if (tx_approval_context.trusted_name_match) {
            pairs[nbPairs].value = g_trusted_name;
            extension.aliasType = ENS_ALIAS;
            extension.title = g_trusted_name;
            extension.fullValue = strings.common.toAddress;
            extension.explanation = strings.common.toAddress;
            pairs[nbPairs].extension = &extension;
            pairs[nbPairs].aliasValue = 1;
        } else {
            pairs[nbPairs].value = strings.common.toAddress;
        }
        nbPairs++;

        // Display the Nonce
        // -----------------
        if (N_storage.displayNonce) {
            pairs[nbPairs].item = "Nonce";
            pairs[nbPairs].value = strings.common.nonce;
            nbPairs++;
        }

        // Display the Max fees
        // --------------------
        pairs[nbPairs].item = "Max fees";
        pairs[nbPairs].value = strings.common.maxFee;
        nbPairs++;

        // Display the Network
        // -------------------
        if (tx_approval_context.displayNetwork) {
            pairs[nbPairs].item = "Network";
            pairs[nbPairs].value = strings.common.network_name;
            nbPairs++;
        }

        // Display the Transaction hash
        // ----------------------------
        if (tmpContent.txContent.dataPresent) {
            // Copy the "0x" prefix
            strlcpy(strings.common.tx_hash, "0x", 3);
            if (bytes_to_lowercase_hex(strings.common.tx_hash + 2,
                                       sizeof(strings.common.tx_hash) - 2,
                                       (const uint8_t *) tmpCtx.transactionContext.hash,
                                       INT256_LENGTH) >= 0) {
#ifdef SCREEN_SIZE_WALLET
                pairs[nbPairs].item = "Transaction hash";
#else
                pairs[nbPairs].item = "Tx hash";
#endif
                pairs[nbPairs].value = strings.common.tx_hash;
                nbPairs++;
            }
        }
    }
    return nbPairs;
}

void ux_approve_tx(bool fromPlugin) {
    uint64_t chain_id = 0;
    uint16_t buf_size = SHARED_BUFFER_SIZE / 2;

    explicit_bzero(&pairsList, sizeof(pairsList));
    explicit_bzero(&tx_approval_context, sizeof(tx_approval_context));
    tx_approval_context.fromPlugin = fromPlugin;

    chain_id = get_tx_chain_id();
    if (chainConfig->chainId == ETHEREUM_MAINNET_CHAINID && chain_id != chainConfig->chainId) {
        tx_approval_context.displayNetwork = true;
    } else {
        tx_approval_context.displayNetwork = false;
    }

    pairsList.nbPairs = setTagValuePairs();
    pairsList.pairs = pairs;

    explicit_bzero(&warning, sizeof(nbgl_warning_t));
    if (tmpContent.txContent.dataPresent) {
        warning.predefinedSet |= SET_BIT(BLIND_SIGNING_WARN);
    }
#ifdef HAVE_WEB3_CHECKS
    set_tx_simulation_warning(&warning, true, true);
#endif

    if (tx_approval_context.fromPlugin) {
        char op_name[sizeof(strings.common.fullAmount)];
        plugin_ui_get_id();

        get_lowercase_operation(op_name, sizeof(op_name));
        snprintf(g_stax_shared_buffer,
                 buf_size,
                 "Review transaction to %s %s%s",
                 op_name,
                 (pluginType == EXTERNAL ? "on " : ""),
                 strings.common.toAddress);
        // Finish text: replace "Review" by "Sign" and add questionmark
#ifdef SCREEN_SIZE_WALLET
        snprintf(g_stax_shared_buffer + buf_size,
                 buf_size,
                 "%s transaction to %s %s%s?",
                 ui_tx_simulation_finish_str(),
                 op_name,
                 (pluginType == EXTERNAL ? "on " : ""),
                 strings.common.toAddress);
#else
        snprintf(g_stax_shared_buffer + buf_size,
                 buf_size,
                 "%s transaction",
                 ui_tx_simulation_finish_str());
#endif
    } else {
        snprintf(g_stax_shared_buffer, buf_size, "Review transaction");
        snprintf(g_stax_shared_buffer + buf_size,
                 buf_size,
#ifdef SCREEN_SIZE_WALLET
                 "%s transaction?",
#else
                 "%s transaction",
#endif
                 ui_tx_simulation_finish_str());
    }

    if (warning.predefinedSet == 0) {
        nbgl_useCaseReview(TYPE_TRANSACTION,
                           &pairsList,
                           get_tx_icon(),
                           g_stax_shared_buffer,
                           NULL,
                           g_stax_shared_buffer + buf_size,
                           reviewChoice);
    } else {
        nbgl_useCaseAdvancedReview(TYPE_TRANSACTION,
                                   &pairsList,
                                   get_tx_icon(),
                                   g_stax_shared_buffer,
                                   NULL,
                                   g_stax_shared_buffer + buf_size,
                                   NULL,
                                   &warning,
                                   reviewChoice);
    }
}
