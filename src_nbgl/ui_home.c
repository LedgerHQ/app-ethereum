#include "common_ui.h"
#include "ui_nbgl.h"
#include "nbgl_use_case.h"
#include "caller_api.h"
#include "network.h"

// settings info definition
#define SETTING_INFO_NB 2

// settings menu definition
#define SETTING_CONTENTS_NB 1

// Tagline format for plugins
#define FORMAT_PLUGIN "This app enables clear\nsigning transactions for\nthe %s dApp."

enum {
    DEBUG_TOKEN = FIRST_USER_TOKEN,
    NONCE_TOKEN,
#ifdef HAVE_EIP712_FULL_SUPPORT
    EIP712_VERBOSE_TOKEN,
#endif
#ifdef HAVE_DOMAIN_NAME
    DOMAIN_NAME_VERBOSE_TOKEN
#endif
};

enum {
#ifdef HAVE_DOMAIN_NAME
    DOMAIN_NAME_VERBOSE_ID,
#endif
#ifdef HAVE_EIP712_FULL_SUPPORT
    EIP712_VERBOSE_ID,
#endif
    NONCE_ID,
    DEBUG_ID,
    SETTINGS_SWITCHES_NB
};

// settings definition
static const char *const infoTypes[SETTING_INFO_NB] = {"Version", "Developer"};
static const char *const infoContents[SETTING_INFO_NB] = {APPVERSION, "Ledger"};

static nbgl_contentInfoList_t infoList = {0};
static nbgl_contentSwitch_t switches[SETTINGS_SWITCHES_NB] = {0};
static nbgl_content_t contents[SETTING_CONTENTS_NB] = {0};
static nbgl_genericContents_t settingContents = {0};

// Buffer used all throughout the NBGL code
char g_stax_shared_buffer[SHARED_BUFFER_SIZE] = {0};

static void setting_toggle_callback(int token, uint8_t index, int page) {
    UNUSED(index);
    UNUSED(page);
    bool value;

    switch (token) {
        case DEBUG_TOKEN:
            value = !N_storage.contractDetails;
            switches[DEBUG_ID].initState = (nbgl_state_t) value;
            nvm_write((void *) &N_storage.contractDetails, (void *) &value, sizeof(uint8_t));
            break;
        case NONCE_TOKEN:
            value = !N_storage.displayNonce;
            switches[NONCE_ID].initState = (nbgl_state_t) value;
            nvm_write((void *) &N_storage.displayNonce, (void *) &value, sizeof(uint8_t));
            break;
#ifdef HAVE_EIP712_FULL_SUPPORT
        case EIP712_VERBOSE_TOKEN:
            value = !N_storage.verbose_eip712;
            switches[EIP712_VERBOSE_ID].initState = (nbgl_state_t) value;
            nvm_write((void *) &N_storage.verbose_eip712, (void *) &value, sizeof(uint8_t));
            break;
#endif  // HAVE_EIP712_FULL_SUPPORT
#ifdef HAVE_DOMAIN_NAME
        case DOMAIN_NAME_VERBOSE_TOKEN:
            value = !N_storage.verbose_domain_name;
            switches[DOMAIN_NAME_VERBOSE_ID].initState = (nbgl_state_t) value;
            nvm_write((void *) &N_storage.verbose_domain_name, (void *) &value, sizeof(uint8_t));
            break;
#endif  // HAVE_DOMAIN_NAME
    }
}

static void app_quit(void) {
    // exit app here
    app_exit();
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

/**
 * Prepare settings, app infos and call the HomeAndSettings use case
 *
 * @param[in] appname given app name
 * @param[in] tagline given tagline (\ref NULL if default)
 */
static void prepare_and_display_home(const char *appname, const char *tagline) {
#ifdef HAVE_DOMAIN_NAME
    switches[DOMAIN_NAME_VERBOSE_ID].initState =
        N_storage.verbose_domain_name ? ON_STATE : OFF_STATE;
    switches[DOMAIN_NAME_VERBOSE_ID].text = "ENS addresses";
    switches[DOMAIN_NAME_VERBOSE_ID].subText = "Display the resolved address of ENS domains.";
    switches[DOMAIN_NAME_VERBOSE_ID].token = DOMAIN_NAME_VERBOSE_TOKEN;
    switches[DOMAIN_NAME_VERBOSE_ID].tuneId = TUNE_TAP_CASUAL;
#endif  // HAVE_DOMAIN_NAME

#ifdef HAVE_EIP712_FULL_SUPPORT
    switches[EIP712_VERBOSE_ID].initState = N_storage.verbose_eip712 ? ON_STATE : OFF_STATE;
    switches[EIP712_VERBOSE_ID].text = "Raw messages";
    switches[EIP712_VERBOSE_ID].subText = "Display raw content from EIP712 messages.";
    switches[EIP712_VERBOSE_ID].token = EIP712_VERBOSE_TOKEN;
    switches[EIP712_VERBOSE_ID].tuneId = TUNE_TAP_CASUAL;
#endif  // HAVE_EIP712_FULL_SUPPORT

    switches[NONCE_ID].initState = N_storage.displayNonce ? ON_STATE : OFF_STATE;
    switches[NONCE_ID].text = "Nonce";
    switches[NONCE_ID].subText = "Display nonce in transactions.";
    switches[NONCE_ID].token = NONCE_TOKEN;
    switches[NONCE_ID].tuneId = TUNE_TAP_CASUAL;

    switches[DEBUG_ID].initState = N_storage.contractDetails ? ON_STATE : OFF_STATE;
    switches[DEBUG_ID].text = "Debug smart contracts";
    switches[DEBUG_ID].subText = "Display contract data details.";
    switches[DEBUG_ID].token = DEBUG_TOKEN;
    switches[DEBUG_ID].tuneId = TUNE_TAP_CASUAL;

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
                                get_app_icon(true),
                                tagline,
                                INIT_HOME_PAGE,
                                &settingContents,
                                &infoList,
                                NULL,
                                app_quit);
}

/**
 * Go to home screen
 *
 * This function prepares the app name & tagline depending on how the application was called
 */
void ui_idle(void) {
    const char *appname = NULL;
    const char *tagline = NULL;

    if (caller_app) {
        appname = caller_app->name;

        if (caller_app->type == CALLER_TYPE_PLUGIN) {
            snprintf(g_stax_shared_buffer, sizeof(g_stax_shared_buffer), FORMAT_PLUGIN, appname);
            tagline = g_stax_shared_buffer;
        }
    } else {  // Ethereum app
        uint64_t mainnet_chain_id = ETHEREUM_MAINNET_CHAINID;
        appname = get_network_name_from_chain_id(&mainnet_chain_id);
    }
    prepare_and_display_home(appname, tagline);
}
