#pragma once

#include "nbgl_use_case.h"
#include "shared_context.h"

#ifdef SCREEN_SIZE_WALLET
#define ICON_APP_WARNING  LARGE_WARNING_ICON
#define ICON_APP_REVIEW   LARGE_REVIEW_ICON
#define ICON_APP_MULTISIG C_multisig_64px
#if defined(TARGET_APEX)
#define ICON_LEDGER C_ledger_48px
#else
#define ICON_LEDGER C_ledger_64px
#endif
#else
#define ICON_LEDGER       C_ledger_14px
#define ICON_APP_WARNING  WARNING_ICON
#define ICON_APP_REVIEW   REVIEW_ICON
#define ICON_APP_MULTISIG C_multisig_14px
#endif

const nbgl_icon_details_t* get_app_icon(bool caller_icon);
const nbgl_icon_details_t* get_tx_icon(bool fromPlugin);

// Global Warning struct for NBGL review flows
extern nbgl_warning_t warning;

void ui_idle(void);
void ui_settings(void);
