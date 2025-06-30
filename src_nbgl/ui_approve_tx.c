#include <ctype.h>
#include "os_utils.h"
#include "apdu_constants.h"
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
#include "mem_utils.h"
#include "ui_utils.h"
#include "enum_value.h"
#include "proxy_info.h"

#define TAG_MAX_LEN   43
#define VALUE_MAX_LEN 79

static nbgl_contentValueExt_t *extension = NULL;

typedef struct {
    char title[TAG_MAX_LEN];
    char msg[VALUE_MAX_LEN];
} plugin_buffers_t;

static plugin_buffers_t *plugin_buffers = NULL;

/**
 * Cleanup allocated memory
 */
static void _cleanup(void) {
    mem_buffer_cleanup((void **) &g_trusted_name);
    mem_buffer_cleanup((void **) &g_trusted_name_info);
    mem_buffer_cleanup((void **) &plugin_buffers);
    mem_buffer_cleanup((void **) &extension);
    ui_all_cleanup();
    enum_value_cleanup();
    proxy_cleanup();
}

// Review callback function to handle user confirmation or cancellation
static void reviewChoice(bool confirm) {
    if (confirm) {
        io_seproxyhal_touch_tx_ok();
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED, ui_idle);
    } else {
        io_seproxyhal_touch_tx_cancel();
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_REJECTED, ui_idle);
    }
    _cleanup();
#ifdef HAVE_TRANSACTION_CHECKS
    clear_tx_simulation();
#endif
}

/**
 * Retrieve the icon for the Transaction
 *
 * @param[in] fromPlugin If true, the data is coming from a plugin, otherwise it is a standard
 * transaction
 * @return Pointer to the icon details structure, or NULL if no icon is available
 */
const nbgl_icon_details_t *get_tx_icon(bool fromPlugin) {
    const nbgl_icon_details_t *icon = NULL;

    if (fromPlugin && (pluginType == EXTERNAL)) {
        if ((caller_app != NULL) && (caller_app->name != NULL)) {
            if (strcmp(strings.common.toAddress, caller_app->name) == 0) {
                icon = get_app_icon(true);
            }
        }
        // icon is NULL in this case
        // Check with Alex if this is expected or a bug
    } else if ((caller_app != NULL) && !fromPlugin) {
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

/**
 * Retrieve the Tag/Value g_pairs to display
 *
 * @param[in] displayNetwork If true, the network name will be displayed
 * @param[in] fromPlugin If true, the data is coming from a plugin, otherwise it is a standard
 * transaction
 */
static void setTagValuePairs(bool displayNetwork, bool fromPlugin) {
    uint8_t nbPairs = 0;
    uint8_t pairIndex = 0;
    uint8_t counter = 0;

    // Setup data to display
    if (fromPlugin) {
        if (pluginType != EXTERNAL) {
            if (strings.common.fromAddress[0] != 0) {
                g_pairs[nbPairs].item = "From";
                g_pairs[nbPairs].value = strings.common.fromAddress;
                nbPairs++;
            }
        }
        for (pairIndex = 0; pairIndex < dataContext.tokenContext.pluginUiMaxItems; pairIndex++) {
            // for the next dataContext.tokenContext.pluginUiMaxItems items, get tag/value from
            // plugin_ui_get_item_internal()
            dataContext.tokenContext.pluginUiCurrentItem = pairIndex;
            plugin_ui_get_item_internal((uint8_t *) plugin_buffers[counter].title,
                                        TAG_MAX_LEN,
                                        (uint8_t *) plugin_buffers[counter].msg,
                                        VALUE_MAX_LEN);
            g_pairs[nbPairs].item = plugin_buffers[counter].title;
            g_pairs[nbPairs].value = plugin_buffers[counter].msg;
            nbPairs++;
            counter++;
        }
        // for the last 1 (or 2), tags are fixed
        if (displayNetwork) {
            g_pairs[nbPairs].item = "Network";
            g_pairs[nbPairs].value = strings.common.network_name;
            nbPairs++;
        }
        g_pairs[nbPairs].item = "Max fees";
        g_pairs[nbPairs].value = strings.common.maxFee;
        nbPairs++;
    } else {
        // Display the From address
        // ------------------------
        if (strings.common.fromAddress[0] != 0) {
            g_pairs[nbPairs].item = "From";
            g_pairs[nbPairs].value = strings.common.fromAddress;
            nbPairs++;
        }

        // Display the Amount
        // ------------------
        if (!tmpContent.txContent.dataPresent ||
            !allzeroes(tmpContent.txContent.value.value, tmpContent.txContent.value.length)) {
            g_pairs[nbPairs].item = "Amount";
            g_pairs[nbPairs].value = strings.common.fullAmount;
            nbPairs++;
        }

        // Display the To address
        // ----------------------
        g_pairs[nbPairs].item = "To";
        if (extension != NULL) {
            g_pairs[nbPairs].value = g_trusted_name;
            extension->aliasType = ENS_ALIAS;
            extension->title = g_trusted_name;
            extension->fullValue = strings.common.toAddress;
            extension->explanation = strings.common.toAddress;
            g_pairs[nbPairs].extension = extension;
            g_pairs[nbPairs].aliasValue = 1;
        } else {
            g_pairs[nbPairs].value = strings.common.toAddress;
        }
        nbPairs++;

        // Display the Nonce
        // -----------------
        if (N_storage.displayNonce) {
            g_pairs[nbPairs].item = "Nonce";
            g_pairs[nbPairs].value = strings.common.nonce;
            nbPairs++;
        }

        // Display the Max fees
        // --------------------
        g_pairs[nbPairs].item = "Max fees";
        g_pairs[nbPairs].value = strings.common.maxFee;
        nbPairs++;

        // Display the Network
        // -------------------
        if (displayNetwork) {
            g_pairs[nbPairs].item = "Network";
            g_pairs[nbPairs].value = strings.common.network_name;
            nbPairs++;
        }

        // Display the Transaction hash
        // ----------------------------
        if ((N_storage.displayHash) || (tmpContent.txContent.dataPresent)) {
            // Copy the "0x" prefix
            strlcpy(strings.common.tx_hash, "0x", 3);
            if (bytes_to_lowercase_hex(strings.common.tx_hash + 2,
                                       sizeof(strings.common.tx_hash) - 2,
                                       (const uint8_t *) tmpCtx.transactionContext.hash,
                                       INT256_LENGTH) >= 0) {
#ifdef SCREEN_SIZE_WALLET
                g_pairs[nbPairs].item = "Transaction hash";
#else
                g_pairs[nbPairs].item = "Tx hash";
#endif
                g_pairs[nbPairs].value = strings.common.tx_hash;
                nbPairs++;
            }
        }
    }
}

/**
 * Computes the number of g_pairs to display in the review screen.
 *
 * @param[in] displayNetwork If true, the network name will be displayed
 * @param[in] fromPlugin If true, the data is coming from a plugin, otherwise it is a standard
 * transaction
 * @return The number of g_pairs to display
 */
static uint8_t getNbPairs(bool displayNetwork, bool fromPlugin) {
    uint8_t nbPairs = 0;

    // Setup data to display
    if (fromPlugin) {
        // Count the From address
        if ((pluginType != EXTERNAL) && (strings.common.fromAddress[0] != 0)) {
            nbPairs++;
        }
        // Count the plugin items
        nbPairs += dataContext.tokenContext.pluginUiMaxItems;
        if (displayNetwork) {
            // Count the Network
            nbPairs++;
        }
        // Count the Max fees
        nbPairs++;
    } else {
        if (strings.common.fromAddress[0] != 0) {
            // Count the From address
            nbPairs++;
        }
        // Count the Amount
        if (!tmpContent.txContent.dataPresent ||
            !allzeroes(tmpContent.txContent.value.value, tmpContent.txContent.value.length)) {
            // This is not displayed if the amount is 0 and data is present
            nbPairs++;
        }
        // Count the To address
        nbPairs++;
        // Count the Nonce
        if (N_storage.displayNonce) {
            nbPairs++;
        }
        // Count the Max fees
        nbPairs++;
        // Count the Network
        if (displayNetwork) {
            nbPairs++;
        }
        // Count the Transaction hash
        if ((N_storage.displayHash) || (tmpContent.txContent.dataPresent)) {
            nbPairs++;
        }
    }
    return nbPairs;
}

/**
 * Initialize the transaction buffers
 *
 * @param[in] fromPlugin If true, the data is coming from a plugin, otherwise it is a standard
 * transaction
 * @param[in] title_len Length of the Title message buffer
 * @param[in] finish_len Length of the Finish message buffer
 * @return whether the initialization was successful
 */
static bool ux_init(bool fromPlugin, uint8_t title_len, uint8_t finish_len) {
    uint64_t chain_id = 0;
    uint16_t buf_size = 0;
    uint8_t nbPairs = 0;
    bool displayNetwork = false;
    e_name_type type = TN_TYPE_ACCOUNT;
    e_name_source source = TN_SOURCE_ENS;

    chain_id = get_tx_chain_id();
    if (chainConfig->chainId == ETHEREUM_MAINNET_CHAINID && chain_id != chainConfig->chainId) {
        displayNetwork = true;
    }
    // Compute the number of g_pairs to display
    nbPairs = getNbPairs(displayNetwork, fromPlugin);

    // Initialize the buffers
    if (!ui_pairs_init(nbPairs)) {
        // Initialization failed, cleanup and return
        goto error;
    }

    // Initialize the buffers
    if (!ui_buffers_init(title_len, 0, finish_len)) {
        // Initialization failed, cleanup and return
        goto error;
    }

    if (fromPlugin == true) {
        buf_size = dataContext.tokenContext.pluginUiMaxItems * sizeof(plugin_buffers_t);
        // Allocate the plugin buffers
        if (mem_buffer_allocate((void **) &plugin_buffers, buf_size) == false) {
            goto error;
        }
    } else if (get_trusted_name(1,
                                &type,
                                1,
                                &source,
                                &chain_id,
                                tmpContent.txContent.destination)) {
        // Allocate the extension memory
        if (mem_buffer_allocate((void **) &extension, sizeof(nbgl_contentValueExt_t)) == false) {
            goto error;
        }
    }

    // Retrieve the Tag/Value g_pairs to display
    setTagValuePairs(displayNetwork, fromPlugin);

    return true;
error:
    io_seproxyhal_send_status(APDU_RESPONSE_INSUFFICIENT_MEMORY, 0, true, true);
    _cleanup();
    return false;
}

/**
 * Display the transaction review screen.
 *
 * @param[in] fromPlugin If true, the data is coming from a plugin, otherwise it is a standard
 * transaction
 */
void ux_approve_tx(bool fromPlugin) {
    char op_name[sizeof(strings.common.fullAmount)];
    const char *title_prefix = "Review transaction";
    const char *tx_check_str = NULL;
    char suffix_str[100];    // Suffix string buffer
    uint8_t title_len = 1;   // Initialize lengths to 1 for '\0' character
    uint8_t finish_len = 1;  // Initialize lengths to 1 for '\0' character

    // Initialize the warning structure
    explicit_bzero(&warning, sizeof(nbgl_warning_t));
    if (tmpContent.txContent.dataPresent) {
        warning.predefinedSet |= SET_BIT(BLIND_SIGNING_WARN);
    }
#ifdef HAVE_TRANSACTION_CHECKS
    set_tx_simulation_warning(&warning, true, true);
#endif
    tx_check_str = ui_tx_simulation_finish_str();

    // Compute the title and finish message lengths
    title_len += strlen(title_prefix);
    finish_len += strlen(tx_check_str);
    finish_len += 12;  // strlen(" transaction");
    if (fromPlugin) {
        plugin_ui_get_id();
        get_lowercase_operation(op_name, sizeof(op_name));

        title_len += 4;  // strlen(" to ");
        title_len += strlen(op_name);
        title_len += strlen((pluginType == EXTERNAL ? " on " : " "));
        title_len += strlen(strings.common.toAddress);

#ifdef SCREEN_SIZE_WALLET
        finish_len += 4;  // strlen(" to ");
        finish_len += strlen(op_name);
        finish_len += strlen((pluginType == EXTERNAL ? " on " : " "));
        finish_len += strlen(strings.common.toAddress);
#endif
    }
#ifdef SCREEN_SIZE_WALLET
    finish_len += 1;  // strlen("?");
#endif

    // Initialize the buffers
    if (!ux_init(fromPlugin, title_len, finish_len)) {
        // Initialization failed, cleanup and return
        return;
    }

    // Prepare the title and finish messages
    strlcpy(g_titleMsg, title_prefix, title_len);
    snprintf(g_finishMsg, finish_len, "%s transaction", tx_check_str);
    if (fromPlugin) {
        // Prepare the suffix
        snprintf(suffix_str,
                 title_len,
                 " to %s %s%s",
                 op_name,
                 (pluginType == EXTERNAL ? "on " : ""),
                 strings.common.toAddress);

        strlcat(g_titleMsg, suffix_str, title_len);

#ifdef SCREEN_SIZE_WALLET
        strlcat(g_finishMsg, suffix_str, finish_len);
#endif
    }
#ifdef SCREEN_SIZE_WALLET
    strlcat(g_finishMsg, "?", finish_len);
#endif

    // Start the review process
    if (warning.predefinedSet == 0) {
        nbgl_useCaseReview(TYPE_TRANSACTION,
                           g_pairsList,
                           get_tx_icon(fromPlugin),
                           g_titleMsg,
                           NULL,
                           g_finishMsg,
                           reviewChoice);
    } else {
        nbgl_useCaseAdvancedReview(TYPE_TRANSACTION,
                                   g_pairsList,
                                   get_tx_icon(fromPlugin),
                                   g_titleMsg,
                                   NULL,
                                   g_finishMsg,
                                   NULL,
                                   &warning,
                                   reviewChoice);
    }
}
