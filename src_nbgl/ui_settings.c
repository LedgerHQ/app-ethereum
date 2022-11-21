#include "common_ui.h"
#include "ui_nbgl.h"
#include "nbgl_use_case.h"

static const char* const infoTypes[] = {"Version", "Ethereum App"};
static const char* const infoContents[] = {APPVERSION, "(c) 2022 Ledger"};

enum {
  BLIND_SIGNING_TOKEN = FIRST_USER_TOKEN,
  DEBUG_TOKEN,
  NONCE_TOKEN,
  EIP712_VERBOSE_TOKEN
};

static nbgl_layoutSwitch_t switches[4];

static bool navCallback(uint8_t page, nbgl_pageContent_t* content) {
  switches[0] = (nbgl_layoutSwitch_t) {
      .initState = N_storage.dataAllowed ? ON_STATE : OFF_STATE,
      .text = "Blind signing",
      .subText = "Enable transaction blind signing",
      .token = BLIND_SIGNING_TOKEN,
      .tuneId = TUNE_TAP_CASUAL
  };
  switches[1] = (nbgl_layoutSwitch_t) {
      .initState = N_storage.contractDetails ? ON_STATE : OFF_STATE,
      .text = "Debug",
      .subText = "Display contract data details",
      .token = DEBUG_TOKEN,
      .tuneId = TUNE_TAP_CASUAL
  };
  switches[2] = (nbgl_layoutSwitch_t) {
      .initState = N_storage.displayNonce ? ON_STATE : OFF_STATE,
      .text = "Nonce",
      .subText = "Display account nonce\nin transaction",
      .token = NONCE_TOKEN,
      .tuneId = TUNE_TAP_CASUAL
  };
  switches[3] = (nbgl_layoutSwitch_t) {
      .initState = N_storage.verbose_eip712 ? ON_STATE : OFF_STATE,
      .text = "Verbose EIP712",
      .subText = "Ignore filtering and\ndisplay raw content",
      .token = EIP712_VERBOSE_TOKEN,
      .tuneId = TUNE_TAP_CASUAL
  };

  switch (page)
  {
    case 0:
      content->type = SWITCHES_LIST;
      content->switchesList.nbSwitches = 4;
      content->switchesList.switches = (nbgl_layoutSwitch_t*)switches;
      break;

    case 1:
      content->type = INFOS_LIST;
      content->infosList.nbInfos = 2;
      content->infosList.infoTypes = (const char**) infoTypes;
      content->infosList.infoContents = (const char**) infoContents;
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
    case EIP712_VERBOSE_TOKEN:
      value = (N_storage.verbose_eip712 ? 0 : 1);
      nvm_write((void*) &N_storage.verbose_eip712, (void*) &value, sizeof(uint8_t));
      break;
  }
}

void ui_menu_settings(void) {
  nbgl_useCaseSettings("Ethereum settings", 0, 2, true, ui_idle, navCallback, controlsCallback);
}
