#include "common_ui.h"
#include "ui_nbgl.h"
#include "nbgl_use_case.h"

static const char* const infoTypes[] = {"Version", APPNAME " App"};
static const char* const infoContents[] = {APPVERSION, "(c) " BUILD_YEAR " Ledger"};

enum {
    BLIND_SIGNING_TOKEN = FIRST_USER_TOKEN,
    DEBUG_TOKEN,
    NONCE_TOKEN,
#ifdef HAVE_EIP712_FULL_SUPPORT
    EIP712_VERBOSE_TOKEN,
#endif  // HAVE_EIP712_FULL_SUPPORT
#ifdef HAVE_DOMAIN_NAME
    DOMAIN_NAME_VERBOSE_TOKEN
#endif  // HAVE_DOMAIN_NAME
};

static nbgl_layoutSwitch_t switches[3];

static bool navCallback(uint8_t page, nbgl_pageContent_t* content) {
    uint8_t index = 0;

    switch (page) {
        case 0:
            content->type = INFOS_LIST;
            content->infosList.nbInfos = 2;
            content->infosList.infoTypes = (const char**) infoTypes;
            content->infosList.infoContents = (const char**) infoContents;
            break;

        case 1:
            switches[index++] =
                (nbgl_layoutSwitch_t){.initState = N_storage.dataAllowed ? ON_STATE : OFF_STATE,
                                      .text = "Blind signing",
                                      .subText = "Enable transaction blind\nsigning",
                                      .token = BLIND_SIGNING_TOKEN,
                                      .tuneId = TUNE_TAP_CASUAL};
            switches[index++] =
                (nbgl_layoutSwitch_t){.initState = N_storage.contractDetails ? ON_STATE : OFF_STATE,
                                      .text = "Debug",
                                      .subText = "Display contract data\ndetails",
                                      .token = DEBUG_TOKEN,
                                      .tuneId = TUNE_TAP_CASUAL};
            switches[index++] =
                (nbgl_layoutSwitch_t){.initState = N_storage.displayNonce ? ON_STATE : OFF_STATE,
                                      .text = "Nonce",
                                      .subText = "Display account nonce\nin transaction",
                                      .token = NONCE_TOKEN,
                                      .tuneId = TUNE_TAP_CASUAL};

            content->type = SWITCHES_LIST;
            content->switchesList.nbSwitches = index;
            content->switchesList.switches = (nbgl_layoutSwitch_t*) switches;
            break;

        case 2:
#ifdef HAVE_EIP712_FULL_SUPPORT
            switches[index++] =
                (nbgl_layoutSwitch_t){.initState = N_storage.verbose_eip712 ? ON_STATE : OFF_STATE,
                                      .text = "Verbose EIP712",
                                      .subText = "Ignore filtering and\ndisplay raw content",
                                      .token = EIP712_VERBOSE_TOKEN,
                                      .tuneId = TUNE_TAP_CASUAL};
#endif  // HAVE_EIP712_FULL_SUPPORT
#ifdef HAVE_DOMAIN_NAME
            switches[index++] = (nbgl_layoutSwitch_t){
                .initState = N_storage.verbose_domain_name ? ON_STATE : OFF_STATE,
                .text = "Verbose domains",
                .subText = "Show resolved address",
                .token = DOMAIN_NAME_VERBOSE_TOKEN,
                .tuneId = TUNE_TAP_CASUAL};
#endif  // HAVE_DOMAIN_NAME

            content->type = SWITCHES_LIST;
            content->switchesList.nbSwitches = index;
            content->switchesList.switches = (nbgl_layoutSwitch_t*) switches;
            break;

        default:
            return false;
            break;
    }

    return true;
}

static void controlsCallback(int token, uint8_t index) {
    (void) index;
    uint8_t value;
    switch (token) {
        case BLIND_SIGNING_TOKEN:
            value = (N_storage.dataAllowed ? 0 : 1);
            nvm_write((void*) &N_storage.dataAllowed, (void*) &value, sizeof(uint8_t));
            break;
        case DEBUG_TOKEN:
            value = (N_storage.contractDetails ? 0 : 1);
            nvm_write((void*) &N_storage.contractDetails, (void*) &value, sizeof(uint8_t));
            break;
        case NONCE_TOKEN:
            value = (N_storage.displayNonce ? 0 : 1);
            nvm_write((void*) &N_storage.displayNonce, (void*) &value, sizeof(uint8_t));
            break;
#ifdef HAVE_EIP712_FULL_SUPPORT
        case EIP712_VERBOSE_TOKEN:
            value = (N_storage.verbose_eip712 ? 0 : 1);
            nvm_write((void*) &N_storage.verbose_eip712, (void*) &value, sizeof(uint8_t));
            break;
#endif  // HAVE_EIP712_FULL_SUPPORT
#ifdef HAVE_DOMAIN_NAME
        case DOMAIN_NAME_VERBOSE_TOKEN:
            value = (N_storage.verbose_domain_name ? 0 : 1);
            nvm_write((void*) &N_storage.verbose_domain_name, (void*) &value, sizeof(uint8_t));
            break;
#endif  // HAVE_DOMAIN_NAME
    }
}

void ui_menu_settings(void) {
    uint8_t nb_screens = 2;
#if defined(HAVE_EIP712_FULL_SUPPORT) || defined(HAVE_DOMAIN_NAME)
    nb_screens += 1;
#endif
    nbgl_useCaseSettings(APPNAME " settings",
                         0,
                         nb_screens,
                         false,
                         ui_idle,
                         navCallback,
                         controlsCallback);
}
