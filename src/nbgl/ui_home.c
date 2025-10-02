#include "nbgl_use_case.h"
#include "ui_nbgl.h"
#include "caller_api.h"
#include "network.h"
#include "cmd_get_tx_simulation.h"
#include "mem_utils.h"
#include "i18n/i18n.h"

// Global Warning struct for NBGL review flows
nbgl_warning_t warning;

// settings info definition
#define SETTING_INFO_NB 3

// settings menu definition
#define SETTING_CONTENTS_NB 1

// Tagline format for plugins
#define FORMAT_PLUGIN "This app enables clear\nsigning transactions for\nthe %s dApp."

enum {
    TRANSACTION_CHECKS_TOKEN = FIRST_USER_TOKEN,
    BLIND_SIGNING_TOKEN,
    NONCE_TOKEN,
    EIP712_VERBOSE_TOKEN,
    EIP7702_TOKEN,
    DEBUG_TOKEN,
    HASH_TOKEN,
    LANGUAGE_TOKEN,
};

enum {
#ifdef HAVE_TRANSACTION_CHECKS
    TRANSACTION_CHECKS_ID,
#endif
    BLIND_SIGNING_ID,
    NONCE_ID,
    EIP712_VERBOSE_ID,
    DEBUG_ID,
    EIP7702_ID,
    HASH_ID,
    LANGUAGE_ID,
    SETTINGS_SWITCHES_NB
};

// settings definition
static const char *infoTypes[SETTING_INFO_NB];
static const char *infoContents[SETTING_INFO_NB];

static nbgl_contentInfoList_t infoList = {0};
static nbgl_contentSwitch_t switches[SETTINGS_SWITCHES_NB] = {0};
static nbgl_content_t contents[SETTING_CONTENTS_NB] = {0};
static nbgl_genericContents_t settingContents = {0};

// Buffer used all throughout the NBGL code
static char *g_tag_line = NULL;

static void setting_toggle_callback(int token, uint8_t index, int page) {
    UNUSED(index);
    UNUSED(page);
    bool value;

    switch (token) {
#ifdef HAVE_TRANSACTION_CHECKS
        case TRANSACTION_CHECKS_TOKEN:
            handle_tx_simulation_opt_in(false);
            value = !N_storage.tx_check_enable;
            switches[TRANSACTION_CHECKS_ID].initState = (nbgl_state_t) value;
            nvm_write((void *) &N_storage.tx_check_enable, (void *) &value, sizeof(value));
            break;
#endif  // HAVE_TRANSACTION_CHECKS
        case BLIND_SIGNING_TOKEN:
            value = !N_storage.dataAllowed;
            switches[BLIND_SIGNING_ID].initState = (nbgl_state_t) value;
            nvm_write((void *) &N_storage.dataAllowed, (void *) &value, sizeof(value));
            break;
        case NONCE_TOKEN:
            value = !N_storage.displayNonce;
            switches[NONCE_ID].initState = (nbgl_state_t) value;
            nvm_write((void *) &N_storage.displayNonce, (void *) &value, sizeof(value));
            break;
        case EIP712_VERBOSE_TOKEN:
            value = !N_storage.verbose_eip712;
            switches[EIP712_VERBOSE_ID].initState = (nbgl_state_t) value;
            nvm_write((void *) &N_storage.verbose_eip712, (void *) &value, sizeof(value));
            break;
        case EIP7702_TOKEN:
            value = !N_storage.eip7702_enable;
            switches[EIP7702_ID].initState = (nbgl_state_t) value;
            nvm_write((void *) &N_storage.eip7702_enable, (void *) &value, sizeof(value));
            break;
        case DEBUG_TOKEN:
            value = !N_storage.contractDetails;
            switches[DEBUG_ID].initState = (nbgl_state_t) value;
            nvm_write((void *) &N_storage.contractDetails, (void *) &value, sizeof(value));
            break;
        case HASH_TOKEN:
            value = !N_storage.displayHash;
            switches[HASH_ID].initState = (nbgl_state_t) value;
            nvm_write((void *) &N_storage.displayHash, (void *) &value, sizeof(value));
            break;
        case LANGUAGE_TOKEN: {
            language_e lang = i18n_get_language();
            lang = (lang == LANG_ENGLISH) ? LANG_FRENCH : LANG_ENGLISH;
            i18n_set_language(lang);
            switches[LANGUAGE_ID].initState = (lang == LANG_FRENCH) ? ON_STATE : OFF_STATE;
            // Refresh the entire UI to apply language change
            ui_idle();
            break;
        }
    }
}

const nbgl_icon_details_t *get_app_icon(bool caller_icon) {
    const nbgl_icon_details_t *icon = NULL;

    if (caller_icon && caller_app) {
        if (caller_app->icon) {
            icon = caller_app->icon;
        }
    } else {
        icon = &ICONGLYPH;
    }
    if (icon == NULL) {
        PRINTF("%s(%s) returned NULL!\n", __func__, (caller_icon ? "true" : "false"));
    }
    return icon;
}

static const nbgl_icon_details_t *get_home_icon(void) {
    const nbgl_icon_details_t *icon = NULL;

    if (caller_app) {
        if (caller_app->icon) {
            icon = caller_app->icon;
        }
    } else {
        icon = &ICONHOME;
    }
    if (icon == NULL) {
        PRINTF("%s returned NULL!\n", __func__);
    }
    return icon;
}

/**
 * Prepare settings, app infos and call the HomeAndSettings use case
 *
 * @param[in] appname given app name
 * @param[in] tagline given tagline (\ref NULL if default)
 * @param[in] page to start on
 */
static void prepare_and_display_home(const char *appname, const char *tagline, uint8_t page) {
    // Initialize info fields with translated strings
    infoTypes[0] = STR(VERSION);
    infoTypes[1] = STR(DEVELOPER);
    infoTypes[2] = STR(COPYRIGHT);
    infoContents[0] = APPVERSION;
    infoContents[1] = "Ledger";
    infoContents[2] = STR(COPYRIGHT);

    switches[BLIND_SIGNING_ID].initState = N_storage.dataAllowed ? ON_STATE : OFF_STATE;
    switches[BLIND_SIGNING_ID].text = STR(BLIND_SIGNING);
    switches[BLIND_SIGNING_ID].subText = STR(BLIND_SIGNING_DESC);
    switches[BLIND_SIGNING_ID].token = BLIND_SIGNING_TOKEN;
#ifdef HAVE_PIEZO_SOUND
    switches[BLIND_SIGNING_ID].tuneId = TUNE_TAP_CASUAL;
#endif

    switches[NONCE_ID].initState = N_storage.displayNonce ? ON_STATE : OFF_STATE;
    switches[NONCE_ID].text = STR(NONCE);
    switches[NONCE_ID].subText = STR(NONCE_DESC);
    switches[NONCE_ID].token = NONCE_TOKEN;
#ifdef HAVE_PIEZO_SOUND
    switches[NONCE_ID].tuneId = TUNE_TAP_CASUAL;
#endif

    switches[EIP712_VERBOSE_ID].initState = N_storage.verbose_eip712 ? ON_STATE : OFF_STATE;
    switches[EIP712_VERBOSE_ID].text = STR(RAW_MESSAGES);
    switches[EIP712_VERBOSE_ID].subText = STR(RAW_MESSAGES_DESC);
    switches[EIP712_VERBOSE_ID].token = EIP712_VERBOSE_TOKEN;
#ifdef HAVE_PIEZO_SOUND
    switches[EIP712_VERBOSE_ID].tuneId = TUNE_TAP_CASUAL;
#endif

    switches[EIP7702_ID].initState = N_storage.eip7702_enable ? ON_STATE : OFF_STATE;
#ifdef SCREEN_SIZE_WALLET
    switches[EIP7702_ID].text = STR(SMART_ACCOUNTS_WALLET);
    switches[EIP7702_ID].subText = STR(SMART_ACCOUNTS_WALLET_DESC);
#else
    switches[EIP7702_ID].text = STR(SMART_ACCOUNTS);
    switches[EIP7702_ID].subText = STR(SMART_ACCOUNTS_DESC);
#endif
    switches[EIP7702_ID].token = EIP7702_TOKEN;
#ifdef HAVE_PIEZO_SOUND
    switches[EIP7702_ID].tuneId = TUNE_TAP_CASUAL;
#endif

    switches[DEBUG_ID].initState = N_storage.contractDetails ? ON_STATE : OFF_STATE;
#ifdef SCREEN_SIZE_WALLET
    switches[DEBUG_ID].text = STR(DEBUG_CONTRACTS_WALLET);
    switches[DEBUG_ID].subText = STR(DEBUG_CONTRACTS_WALLET_DESC);
#else
    switches[DEBUG_ID].text = STR(DEBUG_CONTRACTS);
    switches[DEBUG_ID].subText = STR(DEBUG_CONTRACTS_DESC);
#endif
    switches[DEBUG_ID].token = DEBUG_TOKEN;
#ifdef HAVE_PIEZO_SOUND
    switches[DEBUG_ID].tuneId = TUNE_TAP_CASUAL;
#endif

#ifdef HAVE_TRANSACTION_CHECKS
    switches[TRANSACTION_CHECKS_ID].initState = N_storage.tx_check_enable ? ON_STATE : OFF_STATE;
    switches[TRANSACTION_CHECKS_ID].text = STR(TRANSACTION_CHECK);
    switches[TRANSACTION_CHECKS_ID].subText = STR(TRANSACTION_CHECK_DESC);
    switches[TRANSACTION_CHECKS_ID].token = TRANSACTION_CHECKS_TOKEN;
    switches[TRANSACTION_CHECKS_ID].tuneId = TUNE_TAP_CASUAL;
#endif  // HAVE_TRANSACTION_CHECKS

    switches[HASH_ID].initState = N_storage.displayHash ? ON_STATE : OFF_STATE;
    switches[HASH_ID].text = STR(TRANSACTION_HASH);
#ifdef SCREEN_SIZE_WALLET
    switches[HASH_ID].subText = STR(TRANSACTION_HASH_DESC_WALLET);
#else
    switches[HASH_ID].subText = STR(TRANSACTION_HASH_DESC);
#endif
    switches[HASH_ID].token = HASH_TOKEN;
#ifdef HAVE_PIEZO_SOUND
    switches[HASH_ID].tuneId = TUNE_TAP_CASUAL;
#endif

    switches[LANGUAGE_ID].initState =
        (i18n_get_language() == LANG_FRENCH) ? ON_STATE : OFF_STATE;
    switches[LANGUAGE_ID].text = "Language / Langue";
    switches[LANGUAGE_ID].subText = "English / Français";
    switches[LANGUAGE_ID].token = LANGUAGE_TOKEN;
#ifdef HAVE_PIEZO_SOUND
    switches[LANGUAGE_ID].tuneId = TUNE_TAP_CASUAL;
#endif

    contents[0].type = SWITCHES_LIST;
    contents[0].content.switchesList.nbSwitches = SETTINGS_SWITCHES_NB;
    contents[0].content.switchesList.switches = switches;
    contents[0].contentActionCallback = setting_toggle_callback;

    settingContents.callbackCallNeeded = false;
    settingContents.contentsList = contents;
    settingContents.nbContents = SETTING_CONTENTS_NB;

    infoList.nbInfos = SETTING_INFO_NB;
    infoList.infoTypes = infoTypes;
    infoList.infoContents = infoContents;

    nbgl_useCaseHomeAndSettings(appname,
                                get_home_icon(),
                                tagline,
                                page,
                                &settingContents,
                                &infoList,
                                NULL,
                                app_quit);
}

/**
 * Get appname & tagline
 *
 * This function prepares the app name & tagline depending on how the application was called
 */
static void get_appname_and_tagline(const char **appname, const char **tagline) {
    uint64_t mainnet_chain_id;
    uint8_t line_len = 1;  // Initialize lengths to 1 for '\0' character

    if (caller_app) {
        *appname = caller_app->name;

        if (caller_app->type == CALLER_TYPE_PLUGIN) {
            line_len += strlen(FORMAT_PLUGIN);
            line_len += strlen(caller_app->name);
            // Allocate the buffer - will never be deallocated...
            if (mem_buffer_allocate((void **) &g_tag_line, line_len) == true) {
                snprintf(g_tag_line, line_len, FORMAT_PLUGIN, *appname);
                *tagline = g_tag_line;
            }
        }
    } else {  // Ethereum app
        mainnet_chain_id = ETHEREUM_MAINNET_CHAINID;
        *appname = get_network_name_from_chain_id(&mainnet_chain_id);
    }
}

/**
 * Go to the requested start page
 */
static void ui_start_page(uint8_t page) {
    const char *appname = NULL;
    const char *tagline = NULL;

    get_appname_and_tagline(&appname, &tagline);
    prepare_and_display_home(appname, tagline, page);
}

/**
 * Go to home screen
 */
void ui_idle(void) {
    ui_start_page(INIT_HOME_PAGE);
}

/**
 * Go to settings screen
 */
void ui_settings(void) {
    ui_start_page(0);
}
