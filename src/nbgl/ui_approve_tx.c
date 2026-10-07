#include <ctype.h>
#include "os_utils.h"
#include "apdu_constants.h"
#include "shared_context.h"
#include "common_ui.h"
#include "ui_nbgl.h"
#include "ui_icons.h"
#include "plugins.h"
#include "trusted_name.h"
#include "address_name_lookup.h"
#ifdef HAVE_ADDRESS_BOOK
#include "handle_contacts.h"
#ifdef HAVE_ADDRESS_BOOK_LEDGER_ACCOUNT
#endif  // HAVE_ADDRESS_BOOK_LEDGER_ACCOUNT
#endif  // HAVE_ADDRESS_BOOK
#include "caller_app.h"
#include "network.h"
#include "cmd_get_tx_simulation.h"
#include "cmd_get_gating.h"
#include "utils.h"
#include "app_mem_utils.h"
#include "ui_utils.h"
#include "enum_value.h"
#include "proxy_info.h"

#define TAG_MAX_LEN   43
#define VALUE_MAX_LEN 100

static nbgl_contentValueExt_t *extension = NULL;
static nbgl_contentValueExt_t *from_extension = NULL;

typedef struct {
    char title[TAG_MAX_LEN];
    char msg[VALUE_MAX_LEN];
} plugin_buffers_t;

static plugin_buffers_t *plugin_buffers = NULL;

/**
 * Cleanup allocated memory
 */
static void _cleanup(void) {
    APP_MEM_FREE_AND_NULL((void **) &plugin_buffers);
    APP_MEM_FREE_AND_NULL((void **) &extension);
    APP_MEM_FREE_AND_NULL((void **) &from_extension);
    ui_all_cleanup();
    proxy_cleanup();
#ifdef HAVE_TRANSACTION_CHECKS
    clear_tx_simulation();
#endif
    clear_gating();
}

// Review callback function to handle user confirmation or cancellation
static void reviewChoice(bool confirm) {
    _cleanup();
    if (confirm) {
        io_seproxyhal_touch_tx_ok();
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED, ui_idle);
    } else {
        io_seproxyhal_touch_tx_cancel();
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_REJECTED, ui_idle);
    }
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
 * Resolves a raw Ethereum address to its display name and fills @p pair.
 *
 * Priority: Address Book contact name > ENS trusted name > raw hex string.
 * When a name is found, *ext_out is allocated and the pair carries an alias
 * detail view (tappable on Stax/Flex, shown inline on Nano).
 * The caller is responsible for freeing *ext_out (typically via
 * APP_MEM_FREE_AND_NULL in the cleanup path).
 *
 * @param pair         Tag/value pair whose @p item is already set by the caller
 * @param raw_addr     20-byte raw address to resolve
 * @param display_addr Checksummed hex string; used as fallback value and as
 *                     ENS alias explanation in the detail view
 * @param ext_out      Extension pointer to allocate on match
 * @return false on allocation failure, true otherwise
 */
static bool resolve_address_pair(nbgl_contentTagValue_t *pair,
                                 const uint8_t *raw_addr,
                                 const char *display_addr,
                                 nbgl_contentValueExt_t **ext_out) {
    uint64_t chain_id = get_tx_chain_id();
    const char *display_name = NULL;
    nbgl_contentValueAliasType_t alias_type = 0;
    const char *alias_sub_name = NULL;
    const char *alias_expl = NULL;

#ifdef HAVE_ADDRESS_BOOK
    const s_ab_contact *ab_contact = get_address_book_contact(chain_id, raw_addr);
    // A name that renders as a blank row says nothing in place of the address, so it
    // is not a match: display_name stays clear and the ENS lookup below still runs.
    if ((ab_contact != NULL) && is_displayable_string(ab_contact->contact_name,
                                                      strnlen(ab_contact->contact_name,
                                                              sizeof(ab_contact->contact_name)))) {
        display_name = ab_contact->contact_name;
        alias_type = ADDRESS_BOOK_ALIAS;
        alias_sub_name = (ab_contact->scope[0] != '\0') ? ab_contact->scope : NULL;
    }
    if (display_name == NULL)
#endif  // HAVE_ADDRESS_BOOK
    {
        e_name_type type = TN_TYPE_ACCOUNT;
        e_name_source source = TN_SOURCE_ENS;
        const s_trusted_name *trusted_name =
            get_trusted_name(1, &type, 1, &source, &chain_id, raw_addr);
        if ((trusted_name != NULL) &&
            is_displayable_string(trusted_name->name,
                                  strnlen(trusted_name->name, sizeof(trusted_name->name)))) {
            display_name = trusted_name->name;
            alias_type = ENS_ALIAS;
            alias_expl = display_addr;  // shown as small info text in the detail view
        }
    }

    // No usable name: the checksummed address is the fallback that always says
    // something true.
    if (display_name != NULL) {
        if (APP_MEM_CALLOC((void **) ext_out, sizeof(**ext_out)) == false) {
            return false;
        }
        pair->value = display_name;
        (*ext_out)->aliasType = alias_type;
        (*ext_out)->title = display_name;
        (*ext_out)->fullValue = display_addr;
        (*ext_out)->aliasSubName = alias_sub_name;
        (*ext_out)->explanation = alias_expl;
        pair->extension = *ext_out;
        pair->aliasValue = true;
    } else {
        pair->value = display_addr;
    }
    return true;
}

/**
 * Retrieve the Tag/Value g_pairs to display
 *
 * @param[in] displayNetwork If true, the network name will be displayed
 * @param[in] fromPlugin If true, the data is coming from a plugin, otherwise it is a standard
 * transaction
 */
static bool setTagValuePairs(bool displayNetwork, bool fromPlugin) {
    uint8_t nbPairs = 0;
    uint8_t pairIndex = 0;
    uint8_t counter = 0;

    // Setup data to display
    if (fromPlugin) {
        // Display the From address, external plugins included
        // ----------------------------
        if (strings.common.fromAddress[0] != 0) {
            g_pairs[nbPairs].item = "From";
            if (!resolve_address_pair(&g_pairs[nbPairs],
                                      strings.common.fromAddressRaw,
                                      strings.common.fromAddress,
                                      &from_extension)) {
                return false;
            }
            nbPairs++;
        }
        for (pairIndex = 0; pairIndex < dataContext.tokenContext.pluginUiMaxItems; pairIndex++) {
            // for the next dataContext.tokenContext.pluginUiMaxItems items, get tag/value from
            // plugin_ui_get_item_internal()
            if (nbPairs >= g_pairsList->nbPairs) {
                return false;
            }
            dataContext.tokenContext.pluginUiCurrentItem = pairIndex;
            if (!plugin_ui_get_item_internal((uint8_t *) plugin_buffers[counter].title,
                                             TAG_MAX_LEN,
                                             (uint8_t *) plugin_buffers[counter].msg,
                                             VALUE_MAX_LEN)) {
                return false;
            }
            g_pairs[nbPairs].item = plugin_buffers[counter].title;
            g_pairs[nbPairs].value = plugin_buffers[counter].msg;
            nbPairs++;
            counter++;
        }
        // the last ones are fixed
        if (N_storage.displayNonce) {
            g_pairs[nbPairs].item = "Nonce";
            g_pairs[nbPairs].value = strings.common.nonce;
            nbPairs++;
        }
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
            if (!resolve_address_pair(&g_pairs[nbPairs],
                                      strings.common.fromAddressRaw,
                                      strings.common.fromAddress,
                                      &from_extension)) {
                return false;
            }
            nbPairs++;
        }

        // Display the Amount
        // ------------------
        if (!tmpContent.txContent.dataPresent ||
            !is_zeroes_buffer(tmpContent.txContent.value.value,
                              tmpContent.txContent.value.length)) {
            g_pairs[nbPairs].item = "Amount";
            g_pairs[nbPairs].value = strings.common.fullAmount;
            nbPairs++;
        }

        // Display the To address
        // ----------------------
        // A contract creation carries no destination: the zero bytes left in the buffer
        // are not an address, so nothing is looked up against them and the row keeps the
        // "Contract" value address_to_string() produced.
        const bool has_destination = (tmpContent.txContent.destinationLength != 0);
        g_pairs[nbPairs].item = "To";
#if defined(HAVE_ADDRESS_BOOK) && defined(HAVE_ADDRESS_BOOK_LEDGER_ACCOUNT)
        if (has_destination) {
            const s_ab_contact *to =
                get_address_book_contact(get_tx_chain_id(), tmpContent.txContent.destination);
            if ((to != NULL) && (to->type == AB_CONTACT_LEDGER_ACCOUNT)) {
                g_pairs[nbPairs].item = "To (self transfer)";
            }
        }
#endif  // HAVE_ADDRESS_BOOK && HAVE_ADDRESS_BOOK_LEDGER_ACCOUNT
        if (has_destination) {
            if (!resolve_address_pair(&g_pairs[nbPairs],
                                      tmpContent.txContent.destination,
                                      strings.common.toAddress,
                                      &extension)) {
                return false;
            }
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
    return true;
}

/**
 * Computes the number of g_pairs to display in the review screen.
 *
 * @param[in] displayNetwork If true, the network name will be displayed
 * @param[in] fromPlugin If true, the data is coming from a plugin, otherwise it is a standard
 * transaction
 * @return The number of g_pairs to display
 */
static size_t getNbPairs(bool displayNetwork, bool fromPlugin) {
    size_t nbPairs = 0;

    // Setup data to display
    if (fromPlugin) {
        // Count the From address
        if (strings.common.fromAddress[0] != 0) {
            nbPairs++;
        }
        // Count the plugin items
        nbPairs += dataContext.tokenContext.pluginUiMaxItems;
        if (N_storage.displayNonce) {
            // Count the Nonce
            nbPairs++;
        }
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
            !is_zeroes_buffer(tmpContent.txContent.value.value,
                              tmpContent.txContent.value.length)) {
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
    size_t nbPairs = 0;
    bool displayNetwork = false;

    chain_id = get_tx_chain_id();
    if (g_chain_config->chain_id == ETHEREUM_MAINNET_CHAINID &&
        chain_id != g_chain_config->chain_id) {
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

    if (fromPlugin && (dataContext.tokenContext.pluginUiMaxItems > 0)) {
        buf_size = dataContext.tokenContext.pluginUiMaxItems * sizeof(plugin_buffers_t);
        // Allocate the plugin buffers
        if (APP_MEM_CALLOC((void **) &plugin_buffers, buf_size) == false) {
            goto error;
        }
    }

    // Retrieve the Tag/Value g_pairs to display
    if (!setTagValuePairs(displayNetwork, fromPlugin)) {
        goto error;
    }
    return true;
error:
    io_seproxyhal_send_status(SWO_INSUFFICIENT_MEMORY, 0, true, true);
    _cleanup();
    return false;
}

/**
 * Init the strings for the transaction review screen.
 *
 * @param[in] fromPlugin If true, the data is coming from a plugin, otherwise it is a standard
 * transaction
 * @return status code indicating success or failure
 */
static uint16_t ux_init_strings(bool fromPlugin) {
    char op_name[sizeof(strings.common.fullAmount)];
    const char *title_prefix = "Review transaction";
    const char *tx_check_str = NULL;
    char *suffix_str = NULL;  // Suffix string buffer
    uint8_t title_len = 1;    // Initialize lengths to 1 for '\0' character
    uint8_t finish_len = 1;   // Initialize lengths to 1 for '\0' character

    tx_check_str = ui_tx_simulation_finish_str();

    // Compute the title and finish message lengths
    title_len += strlen(title_prefix);
    finish_len += strlen(tx_check_str);
    finish_len += 12;  // strlen(" transaction");
    if (fromPlugin) {
        if (!plugin_ui_get_id()) {
            return SWO_INCORRECT_DATA;
        }
        get_lowercase_operation(op_name, sizeof(op_name));

        title_len += 4;  // strlen(" to ");
        title_len += strlen(op_name);
        title_len += strlen((pluginType == PLUGIN_TYPE_EXTERNAL ? " on " : " "));
        title_len += strlen(strings.common.toAddress);

#ifdef SCREEN_SIZE_WALLET
        finish_len += 4;  // strlen(" to ");
        finish_len += strlen(op_name);
        finish_len += strlen((pluginType == PLUGIN_TYPE_EXTERNAL ? " on " : " "));
        finish_len += strlen(strings.common.toAddress);
#endif
    }
#ifdef SCREEN_SIZE_WALLET
    finish_len += 1;  // strlen("?");
#endif

    // Initialize the buffers
    if (!ux_init(fromPlugin, title_len, finish_len)) {
        // Initialization failed, cleanup and return
        return SWO_INSUFFICIENT_MEMORY;
    }

    // Prepare the title and finish messages
    strlcpy(g_titleMsg, title_prefix, title_len);
    snprintf(g_finishMsg, finish_len, "%s transaction", tx_check_str);
    if (fromPlugin) {
        // Prepare the suffix
        if ((suffix_str = APP_MEM_ALLOC(title_len)) == NULL) {
            _cleanup();
            return SWO_INSUFFICIENT_MEMORY;
        }
        snprintf(suffix_str,
                 title_len,
                 " to %s %s%s",
                 op_name,
                 (pluginType == PLUGIN_TYPE_EXTERNAL ? "on " : ""),
                 strings.common.toAddress);

        strlcat(g_titleMsg, suffix_str, title_len);

#ifdef SCREEN_SIZE_WALLET
        strlcat(g_finishMsg, suffix_str, finish_len);
#endif
        APP_MEM_FREE(suffix_str);
    }
#ifdef SCREEN_SIZE_WALLET
    strlcat(g_finishMsg, "?", finish_len);
#endif

    return SWO_SUCCESS;
}

/**
 * Display the transaction review screen.
 *
 * @param[in] fromPlugin If true, the data is coming from a plugin, otherwise it is a standard
 * transaction
 * @return status code indicating success or failure
 */
uint16_t ux_approve_tx(bool fromPlugin) {
    uint16_t sw = SWO_PARAMETER_ERROR_NO_INFO;
    // Initialize the warning structure
    explicit_bzero(&warning, sizeof(nbgl_warning_t));
    if (tmpContent.txContent.dataPresent) {
        warning.predefinedSet |= SET_BIT(BLIND_SIGNING_WARN);
        warning.predefinedSet |= SET_BIT(GATED_SIGNING_WARN);
        if (set_gating_warning() == false) {
            return SWO_INCORRECT_DATA;
        }
    }
#ifdef HAVE_TRANSACTION_CHECKS
    set_tx_simulation_warning();
#endif

    // Initialize the strings
    sw = ux_init_strings(fromPlugin);
    if (sw != SWO_SUCCESS) {
        return sw;
    }

    nbgl_useCaseAdvancedReview(TYPE_TRANSACTION,
                               g_pairsList,
                               get_tx_icon(fromPlugin),
                               g_titleMsg,
                               NULL,
                               g_finishMsg,
                               NULL,
                               &warning,
                               reviewChoice);
    return SWO_SUCCESS;
}
