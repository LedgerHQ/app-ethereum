#pragma once

#include "nbgl_use_case.h"
#include "shared_context.h"
#include "glyphs.h"

#ifndef LARGE_REVIEW_ICON
    // TODO: SDK should really define this
    #ifdef TARGET_APEX
        #define LARGE_REVIEW_ICON C_Review_48px
    #else
        #define LARGE_REVIEW_ICON C_Review_64px
    #endif
#endif

#ifdef SCREEN_SIZE_WALLET
    #define ICON_APP_WARNING LARGE_WARNING_ICON
    #define ICON_APP_REVIEW  LARGE_REVIEW_ICON
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
