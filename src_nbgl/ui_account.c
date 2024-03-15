#include <stdbool.h>
#include "common_ui.h"
#include "ui_nbgl.h"
#include "nbgl_use_case.h"
#include "nbgl_layout.h"
#include "feature_favoriteAccounts.h"
#include "network.h"
#include "network_icons.h"
#include "ledger_assert.h"

/*********************
 *      DEFINES
 *********************/
/**********************
 *      TYPEDEFS
 **********************/
enum {
    TOKEN_ACCOUNT_BACK = FIRST_USER_TOKEN,
    TOKEN_ACCOUNT_ADD,
    TOKEN_ACCOUNT_DEL,
    TOKEN_ACCOUNT_OPTIONS,
    TOKEN_ACCOUNT_1,
    TOKEN_ACCOUNT_2,
    TOKEN_ACCOUNT_3,
    TOKEN_ACCOUNT_4,
    TOKEN_ACCOUNT_5
};

/**********************
 *  STATIC VARIABLES
 **********************/
// contexts for background and modal pages
static nbgl_layout_t layoutCtx = {0};

/**********************
 *  STATIC FUNCTIONS
 **********************/
static void account_cb(int token, uint8_t index);

/**
 * @brief Return a short address, limited in size, with '...' in the middle.
 *
 * @param address Full address value
 * @param maxLength Maximum nb of characters (without 0x)
 * @return The new formatted string
 */
static char *formatShortAddress(const char *address, uint8_t maxLength) {
    uint8_t len, nbChars, offset;
    char begin[ADDRESS_LENGTH + 2] = {0};  // keeping 0x
    char end[ADDRESS_LENGTH] = {0};
    static char addr_short[ADDRESS_LENGTH + 2];  // Adding 0x

    // Ensure maxLength will fit in resulting string
    LEDGER_ASSERT(maxLength <= sizeof(addr_short), "Bad ShortAddress size!");

    // Computes internal variables, for better readability
    len = strlen(address);
    nbChars = (maxLength - 4) / 2;  // 4 corresponds to the middle '....'
    offset = len - nbChars;
    // Retrieve the new beginning part
    memcpy(begin, address, nbChars + 2);  // keeping 0x
    // Retrieve the new ending part
    memcpy(end, address + offset, nbChars);
    // Generate the full short string
    snprintf(addr_short, sizeof(addr_short), "%s....%s", begin, end);
    return addr_short;
}

/**
 * @brief Callback called when 'Options' button is pressed on the 'My accounts' page
 *
 * @param page Selected page to populate
 * @param content Output structure describing the page content to display
 * @return 'true' if the page content has been filled
 */
static bool options_nav_cb(uint8_t page, nbgl_pageContent_t *content) {
    UNUSED(page);
    static const char *const barTexts[] = {"Import more accounts", "Remove all accounts"};
    static const uint8_t barTokens[] = {TOKEN_ACCOUNT_ADD, TOKEN_ACCOUNT_DEL};
    static nbgl_icon_details_t *icons[2] = {0};

    // Init icons list
    icons[0] = PIC(&C_plus32px);
    icons[1] = PIC(&C_trash32px);

    // Init page content with bars containing both test and icons
    memset(content, 0, sizeof(nbgl_pageContent_t));
    content->type = BARS_LIST_ICONS;
    content->barsListIcons.barTexts = barTexts;
    content->barsListIcons.tokens = barTokens;
    content->barsListIcons.nbBars = (getNbFavoriteAccounts() == 0) ? 1 : ARRAYLEN(barTokens);
    content->barsListIcons.barIcons = (const nbgl_icon_details_t *const *) icons;
    content->barsListIcons.tuneId = TUNE_TAP_CASUAL;
    return true;
}

/**
 * @brief Callback called when selecting 'Remove' on the 'Options' page
 *
 * @param confirm If true, means that the confirmation button has been pressed
 */
static void del_cb(bool confirm) {
    if (confirm) {
        deleteFavoriteAccounts();
    }
    ui_menu_account();
}

/**
 * @brief Callback called when selecting 'Import' on the 'Options' page
 *
 * @param token Integer identifying the touched control widget
 * @param index Value of the activated widget (for radio buttons, switches...)
 */
static void add_cb(int token, uint8_t index) {
    UNUSED(index);

    switch (token) {
        // 'Back' button widget
        case TOKEN_ACCOUNT_BACK:
            // Get back on 'My accounts' page
            // ui_menu_account();
            // Get back on 'Options' page
            account_cb(TOKEN_ACCOUNT_OPTIONS, 0);
            break;
    }
}

/**
 * @brief Callback called when navigating on the 'Options' page
 *
 * @param token Integer identifying the touched control widget
 * @param index Value of the activated widget (for radio buttons, switches...)
 */
static void options_ctrl_cb(int token, uint8_t index) {
    UNUSED(index);
    nbgl_layoutCenteredInfo_t centeredInfo = {0};
    nbgl_layoutDescription_t layoutDescription = {0};
    nbgl_layoutQRCode_t layoutQRCode = {0};
    int status = -1;

    switch (token) {
        // Widget to 'Import more accounts'
        case TOKEN_ACCOUNT_ADD:
            // Add page layout
            layoutDescription.onActionCallback = add_cb;
            layoutDescription.modal = false;
            layoutCtx = nbgl_layoutGet(&layoutDescription);

            // Add description
            centeredInfo.text1 = "Import accounts";
            centeredInfo.text2 = "Scan this QR code to open Ledger Live and import more accounts";
            centeredInfo.style = LARGE_CASE_INFO;
            centeredInfo.onTop = true;
            nbgl_layoutAddCenteredInfo(layoutCtx, &centeredInfo);

            // Add QR code
            layoutQRCode.url = "ledger.com/start/my-ledger";
            nbgl_layoutAddQRCode(layoutCtx, &layoutQRCode);

            // Add bottom exit button
            status = nbgl_layoutAddBottomButton(layoutCtx,
                                                PIC(&C_cross32px),
                                                TOKEN_ACCOUNT_BACK,
                                                false,
                                                TUNE_TAP_CASUAL);
            if (status < 0) {
                return;
            }
            // Draw the page
            nbgl_layoutDraw(layoutCtx);
            break;

        // Widget to 'Remove all accounts'
        case TOKEN_ACCOUNT_DEL:
            // Draw the confirmation page, with its callback
            nbgl_useCaseChoice(&C_trash32px,
                               "Remove accounts from this Ledger Stax",
                               "If you change your mind, you'll be able "
                               "to synchronize them again from Ledger Live.",
                               "Remove accounts",
                               "Cancel",
                               del_cb);
            break;
        default:
            break;
    }
}

/**
 * @brief Callback called when navigating on a 'Account' dedicated page
 *
 * @param token Integer identifying the touched control widget
 * @param index Value of the activated widget (for radio buttons, switches...)
 */
static void single_account_cb(int token, uint8_t index) {
    UNUSED(index);

    switch (token) {
        // 'Back' button widget
        case TOKEN_ACCOUNT_BACK:
            // Get back on 'My accounts' page
            ui_menu_account();
            break;
    }
}

/**
 * @brief Callback called when navigating on 'My Accounts' page
 *
 * @param token Integer identifying the touched control widget
 * @param index Value of the activated widget (for radio buttons, switches...)
 */
static void account_cb(int token, uint8_t index) {
    UNUSED(index);
    nbgl_layoutCenteredInfo_t centeredInfo = {0};
    nbgl_layoutDescription_t layoutDescription = {0};
    nbgl_layoutQRCode_t layoutQRCode = {0};
    int status = -1;
    uint8_t account;
    static char network[64];
    char *network_name = NULL;

    switch (token) {
        // 'Back' button in the top bar
        case TOKEN_ACCOUNT_BACK:
            // Back to main App screen
            ui_idle();
            break;

        // 'Options' button in the footer bar
        case TOKEN_ACCOUNT_OPTIONS:
            // Start a new screen for accounts management
            nbgl_useCaseSettings("My Accounts",
                                 0,
                                 1,
                                 false,
                                 ui_menu_account,
                                 options_nav_cb,
                                 options_ctrl_cb);
            break;

        // 'Accounts' dedicated bars
        case TOKEN_ACCOUNT_1:
        case TOKEN_ACCOUNT_2:
        case TOKEN_ACCOUNT_3:
        case TOKEN_ACCOUNT_4:
        case TOKEN_ACCOUNT_5:
            // Add page layout
            layoutDescription.onActionCallback = single_account_cb;
            layoutDescription.modal = false;
            layoutCtx = nbgl_layoutGet(&layoutDescription);

            // Add description
            account = token - TOKEN_ACCOUNT_1;
            centeredInfo.text1 = (const char *) N_accounts.accounts[account].name;
            centeredInfo.style = LARGE_CASE_INFO;
            centeredInfo.onTop = true;
            nbgl_layoutAddCenteredInfo(layoutCtx, &centeredInfo);

            // Init strings to be displayed
            network_name = (char *) get_network_name_from_chain_id(
                (const uint64_t *) &(N_accounts.accounts[account].chain_id));

            // Add QR code
            layoutQRCode.url = (const char *) N_accounts.accounts[account].address;
            layoutQRCode.text2 = (const char *) N_accounts.accounts[account].address;
            if (network_name != NULL) {
                snprintf(network, sizeof(network), "Network: %s", (const char *) network_name);
                layoutQRCode.text1 = (const char *) network;
                layoutQRCode.largeText1 = true;
                nbgl_layoutAddQRCodeIcon(layoutCtx,
                                         &layoutQRCode,
                                         get_network_small_icon_from_chain_id((const uint64_t *) &(
                                             N_accounts.accounts[account].chain_id)));
            } else {
                nbgl_layoutAddQRCode(layoutCtx, &layoutQRCode);
            }

            // Add bottom exit button
            status = nbgl_layoutAddBottomButton(layoutCtx,
                                                PIC(&C_cross32px),
                                                TOKEN_ACCOUNT_BACK,
                                                false,
                                                TUNE_TAP_CASUAL);
            if (status < 0) {
                return;
            }
            // Draw the page
            nbgl_layoutDraw(layoutCtx);
            break;
        default:
            break;
    }
}

/**
 * @brief Callback called to update several entries of the Address Book
 *
 * @param confirm If true, means that the confirmation button has been pressed
 */
static void sync_cb(bool confirm) {
    uint8_t selAccount = 0;
    uint8_t i = 0;

    if (confirm) {
        for (i = 0; i < accountsUpdate.nbAccounts; i++) {
            // Determine current offset to write, if it is a Raname or a Add
            if (accountsUpdate.existIndex[i] != INVALID_ACCOUNT_INDEX) {
                selAccount = accountsUpdate.existIndex[i];
            } else {
                selAccount = getNbFavoriteAccounts();
            }
            nvm_write((void *) &N_accounts.accounts[selAccount],
                      &accountsUpdate.accounts[i],
                      sizeof(AccountData_t));
        }
        nbgl_useCaseStatus("ADDRESS BOOK UPDATED", true, ui_menu_account);
    }
    memset(&accountsUpdate, 0, sizeof(AccountUpdate_t));
    dumpAllAccounts();
    ui_idle();
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief Called to confirm account(s) modification in the Address Book
 *
 */
void ui_account_sync(void) {
    static nbgl_layoutTagValue_t tagValuePair[4 * ACCOUNTS_MAX] = {0};
    nbgl_layoutTagValueList_t tagValueList = {0};
    nbgl_pageInfoLongPress_t infoLongPress = {0};
    uint8_t nbpairs = 0;
    uint8_t i = 0;
    uint8_t nbRenames = 0;
    uint8_t nbNetworks = 1;  // At least the network for the 1st account data

    // Single/Multi Rename only or Add
    for (i = 0; i < accountsUpdate.nbAccounts; i++) {
        if (accountsUpdate.existIndex[0] != INVALID_ACCOUNT_INDEX) {
            nbRenames++;
        }
        if (accountsUpdate.accounts[i].chain_id != accountsUpdate.accounts[0].chain_id) {
            nbNetworks++;
        }
    }

    // Last page configuration
    if (nbRenames == accountsUpdate.nbAccounts) {
        // Only rename operation(s)
        if (accountsUpdate.nbAccounts == 1) {
            infoLongPress.text = "Rename account?";
        } else {
            infoLongPress.text = "Rename accounts?";
        }
        infoLongPress.longPressText = "Rename";
    } else if (getNbFavoriteAccounts() == 0) {
        // Address Book is currently empty
        infoLongPress.text = "Save Address book?";
        infoLongPress.longPressText = "Save";
    } else {
        // At least 1 add operation
        infoLongPress.text = "Update Address book?";
        infoLongPress.longPressText = "Update";
    }
    if ((accountsUpdate.nbAccounts == 1) || (nbNetworks == 1)) {
        // Single account or single network
        infoLongPress.icon = get_network_small_icon_from_chain_id(
            (const uint64_t *) &(accountsUpdate.accounts[0].chain_id));
    } else {
        // Use Ethereum default network icon
        infoLongPress.icon = &C_stax_chain_1_64px;
    }

    // Tag/Value pairs init
    for (i = 0; i < accountsUpdate.nbAccounts; i++) {
        const char *network_name = (const char *) get_network_name_from_chain_id(
            (const uint64_t *) &(accountsUpdate.accounts[i].chain_id));

        // Start on a new page
        tagValuePair[nbpairs].force_page_start = 1;
        // Tag/Value pairs to display: Operation type
        tagValuePair[nbpairs].item = "Operation";
        if (accountsUpdate.existIndex[i] == INVALID_ACCOUNT_INDEX) {
            // This is a new account
            tagValuePair[nbpairs].value = "Add account";
        } else {
            // This is an existing account
            tagValuePair[nbpairs].value = "Rename account";
        }
        nbpairs++;
        // Tag/Value pairs to display: Network
        if (network_name != NULL) {
            tagValuePair[nbpairs].item = "Network";
            tagValuePair[nbpairs].value = network_name;
            nbpairs++;
        }
        // Tag/Value pairs to display: Account name
        if (accountsUpdate.existIndex[i] == INVALID_ACCOUNT_INDEX) {
            // This is a new account: print its requested name
            tagValuePair[nbpairs].item = "Name";
            tagValuePair[nbpairs].value = accountsUpdate.accounts[i].name;
        } else {
            // This is an existing account: print its current name
            tagValuePair[nbpairs].item = "Current Name";
            tagValuePair[nbpairs].value =
                (const char *) N_accounts.accounts[accountsUpdate.existIndex[i]].name;
        }
        nbpairs++;
        // Tag/Value pairs to display: Address or New Name
        if (accountsUpdate.existIndex[i] == INVALID_ACCOUNT_INDEX) {
            // This is a new account: print its address
            tagValuePair[nbpairs].item = "Address";
            tagValuePair[nbpairs].value = accountsUpdate.accounts[i].address;
        } else {
            // This is an existing account: print its new requested name
            tagValuePair[nbpairs].item = "New Name";
            tagValuePair[nbpairs].value = accountsUpdate.accounts[i].name;
        }
        nbpairs++;
    }

    // Static review
    tagValueList.pairs = tagValuePair;
    tagValueList.nbPairs = nbpairs;
    tagValueList.startIndex = 0;
    tagValueList.nbMaxLinesForValue = 3;
    tagValueList.smallCaseForValue = true;
    tagValueList.wrapping = true;

    nbgl_useCaseStaticReviewLight(&tagValueList, &infoLongPress, "Cancel", sync_cb);
}

/**
 * @brief Called from the App main screen to manage the accounts
 */
void ui_menu_account(void) {
    nbgl_layoutDescription_t layoutDescription = {0};
    nbgl_layoutBar_t bar = {0};
    int status = -1;
    uint8_t i = 0;
    static char subtext[ACCOUNTS_MAX][ACCOUNT_NAME_MAX_LENGTH + 1] = {0};

    // Add page layout
    layoutDescription.onActionCallback = account_cb;
    layoutDescription.modal = false;
    layoutCtx = nbgl_layoutGet(&layoutDescription);

    // Add bottom option button
    status = nbgl_layoutAddBottomButton(layoutCtx,
                                        PIC(&C_filter32px),
                                        TOKEN_ACCOUNT_OPTIONS,
                                        true,
                                        TUNE_TAP_CASUAL);
    if (status < 0) {
        return;
    }

    // Add title touchable bar
    memset(&bar, 0, sizeof(nbgl_layoutBar_t));
    bar.text = "My accounts";
    bar.iconLeft = &C_leftArrow32px;
    bar.token = TOKEN_ACCOUNT_BACK;
    bar.centered = true;
    bar.tuneId = TUNE_TAP_CASUAL;
    nbgl_layoutAddTouchableBar(layoutCtx, &bar);
    nbgl_layoutAddSeparationLine(layoutCtx);

    // Add available accounts
    for (i = 0; i < getNbFavoriteAccounts(); i++) {
        memset(&bar, 0, sizeof(nbgl_layoutBar_t));
        snprintf(
            subtext[i],
            sizeof(subtext[i]),
            "%s",
            (const char *) formatShortAddress((const char *) N_accounts.accounts[i].address, 12));
        bar.text = (const char *) N_accounts.accounts[i].name;
        bar.subText = (const char *) subtext[i];
        bar.iconRight = &C_Next32px;
        bar.iconLeft = get_network_small_icon_from_chain_id(
            (const uint64_t *) &(N_accounts.accounts[i].chain_id));
        bar.token = TOKEN_ACCOUNT_1 + i;
        bar.centered = false;
        bar.tuneId = TUNE_TAP_CASUAL;
        nbgl_layoutAddTouchableBar(layoutCtx, &bar);
        nbgl_layoutAddSeparationLine(layoutCtx);
    }

    // Draw the page
    nbgl_layoutDraw(layoutCtx);
}
