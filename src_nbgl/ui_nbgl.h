#pragma once

#include "nbgl_use_case.h"
#include "shared_context.h"
#include "glyphs.h"

#ifdef SCREEN_SIZE_WALLET
#define ICON_APP_WARNING C_Warning_64px
#define ICON_APP_REVIEW  C_Review_64px
#else
#define ICON_APP_WARNING C_icon_warning
#define ICON_APP_REVIEW  C_icon_certificate
#endif

const nbgl_icon_details_t* get_app_icon(bool caller_icon);
const nbgl_icon_details_t* get_tx_icon(bool fromPlugin);

// Global Warning struct for NBGL review flows
extern nbgl_warning_t warning;

void ui_idle(void);
void ui_settings(void);
