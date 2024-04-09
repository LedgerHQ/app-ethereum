#include "common_ui.h"
#include "ui_nbgl.h"
#include "nbgl_use_case.h"

// settings info definition
#define SETTING_INFO_NB 2

// settings menu definition
#define SETTING_CONTENTS_NB 1

enum {
    BLIND_SIGNING_TOKEN = FIRST_USER_TOKEN,
    DEBUG_TOKEN,
    NONCE_TOKEN,
#ifdef HAVE_EIP712_FULL_SUPPORT
    EIP712_VERBOSE_TOKEN,
#endif
#ifdef HAVE_DOMAIN_NAME
    DOMAIN_NAME_VERBOSE_TOKEN
#endif
};

enum {
    BLIND_SIGNING_ID = 0,
    DEBUG_ID,
    NONCE_ID,
#ifdef HAVE_EIP712_FULL_SUPPORT
    EIP712_VERBOSE_ID,
#endif
#ifdef HAVE_DOMAIN_NAME
    DOMAIN_NAME_VERBOSE_ID,
#endif
    SETTINGS_SWITCHES_NB
};

static uint8_t initSettingPage;

// settings definition
static const char* const infoTypes[SETTING_INFO_NB] = {"Version", APPNAME " App"};
static const char* const infoContents[SETTING_INFO_NB] = {APPVERSION, "(c) " BUILD_YEAR " Ledger"};

static nbgl_contentInfoList_t infoList = {0};
static nbgl_contentSwitch_t switches[SETTINGS_SWITCHES_NB] = {0};
static nbgl_content_t contents[SETTING_CONTENTS_NB] = {0};
static nbgl_genericContents_t settingContents = {0};

static void controlsCallback(int token, uint8_t index, int page) {
    UNUSED(index);
    uint8_t value;

    initSettingPage = page;

    switch (token) {
        case BLIND_SIGNING_TOKEN:
            value = (N_storage.dataAllowed ? 0 : 1);
            switches[BLIND_SIGNING_ID].initState = (nbgl_state_t) value;
            nvm_write((void*) &N_storage.dataAllowed, (void*) &value, sizeof(uint8_t));
            break;
        case DEBUG_TOKEN:
            value = (N_storage.contractDetails ? 0 : 1);
            switches[DEBUG_ID].initState = (nbgl_state_t) value;
            nvm_write((void*) &N_storage.contractDetails, (void*) &value, sizeof(uint8_t));
            break;
        case NONCE_TOKEN:
            value = (N_storage.displayNonce ? 0 : 1);
            switches[NONCE_ID].initState = (nbgl_state_t) value;
            nvm_write((void*) &N_storage.displayNonce, (void*) &value, sizeof(uint8_t));
            break;
#ifdef HAVE_EIP712_FULL_SUPPORT
        case EIP712_VERBOSE_TOKEN:
            value = (N_storage.verbose_eip712 ? 0 : 1);
            switches[EIP712_VERBOSE_ID].initState = (nbgl_state_t) value;
            nvm_write((void*) &N_storage.verbose_eip712, (void*) &value, sizeof(uint8_t));
            break;
#endif  // HAVE_EIP712_FULL_SUPPORT
#ifdef HAVE_DOMAIN_NAME
        case DOMAIN_NAME_VERBOSE_TOKEN:
            value = (N_storage.verbose_domain_name ? 0 : 1);
            switches[DOMAIN_NAME_VERBOSE_ID].initState = (nbgl_state_t) value;
            nvm_write((void*) &N_storage.verbose_domain_name, (void*) &value, sizeof(uint8_t));
            break;
#endif  // HAVE_DOMAIN_NAME
    }
}

void ui_menu_settings(void) {
    switches[BLIND_SIGNING_ID].initState = N_storage.dataAllowed ? ON_STATE : OFF_STATE;
    switches[BLIND_SIGNING_ID].text = "Blind signing";
    switches[BLIND_SIGNING_ID].subText = "Enable transaction blind\nsigning";
    switches[BLIND_SIGNING_ID].token = BLIND_SIGNING_TOKEN;
    switches[BLIND_SIGNING_ID].tuneId = TUNE_TAP_CASUAL;

    switches[DEBUG_ID].initState = N_storage.contractDetails ? ON_STATE : OFF_STATE;
    switches[DEBUG_ID].text = "Debug";
    switches[DEBUG_ID].subText = "Display contract data\ndetails";
    switches[DEBUG_ID].token = DEBUG_TOKEN;
    switches[DEBUG_ID].tuneId = TUNE_TAP_CASUAL;

    switches[NONCE_ID].initState = N_storage.displayNonce ? ON_STATE : OFF_STATE;
    switches[NONCE_ID].text = "Nonce";
    switches[NONCE_ID].subText = "Display account nonce\nin transaction";
    switches[NONCE_ID].token = NONCE_TOKEN;
    switches[NONCE_ID].tuneId = TUNE_TAP_CASUAL;

#ifdef HAVE_EIP712_FULL_SUPPORT
    switches[EIP712_VERBOSE_ID].initState = N_storage.verbose_eip712 ? ON_STATE : OFF_STATE;
    switches[EIP712_VERBOSE_ID].text = "Verbose EIP712";
    switches[EIP712_VERBOSE_ID].subText = "Ignore filtering and\ndisplay raw content";
    switches[EIP712_VERBOSE_ID].token = EIP712_VERBOSE_TOKEN;
    switches[EIP712_VERBOSE_ID].tuneId = TUNE_TAP_CASUAL;
#endif  // HAVE_EIP712_FULL_SUPPORT

#ifdef HAVE_DOMAIN_NAME
    switches[DOMAIN_NAME_VERBOSE_ID].initState =
        N_storage.verbose_domain_name ? ON_STATE : OFF_STATE;
    switches[DOMAIN_NAME_VERBOSE_ID].text = "Verbose domains";
    switches[DOMAIN_NAME_VERBOSE_ID].subText = "Show resolved address";
    switches[DOMAIN_NAME_VERBOSE_ID].token = DOMAIN_NAME_VERBOSE_TOKEN;
    switches[DOMAIN_NAME_VERBOSE_ID].tuneId = TUNE_TAP_CASUAL;
#endif  // HAVE_DOMAIN_NAME

    contents[0].type = SWITCHES_LIST;
    contents[0].content.switchesList.nbSwitches = SETTINGS_SWITCHES_NB;
    contents[0].content.switchesList.switches = switches;
    contents[0].contentActionCallback = controlsCallback;

    settingContents.callbackCallNeeded = false;
    settingContents.contentsList = contents;
    settingContents.nbContents = SETTING_CONTENTS_NB;

    infoList.nbInfos = SETTING_INFO_NB;
    infoList.infoTypes = infoTypes;
    infoList.infoContents = infoContents;

    nbgl_useCaseHomeAndSettings(APPNAME,
                                get_app_icon(true),
                                NULL,
                                initSettingPage,
                                &settingContents,
                                &infoList,
                                NULL,
                                app_quit);
}
