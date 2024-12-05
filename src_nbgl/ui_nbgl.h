#ifndef _UI_NBGL_H_
#define _UI_NBGL_H_

#include "nbgl_use_case.h"
#include "shared_context.h"
#include "glyphs.h"

#ifdef SCREEN_SIZE_WALLET
#define ICON_APP_WARNING C_Warning_64px
#define ICON_APP_REVIEW  C_Review_64px
#if defined(TARGET_STAX)
#define ICON_APP_REVIEW_INFO C_Question_32px
#else
#define ICON_APP_REVIEW_INFO C_Question_40px
#endif
#else
#define ICON_APP_WARNING     C_icon_warning
#define ICON_APP_REVIEW      C_icon_certificate
#define ICON_APP_REVIEW_INFO C_icon_eye
#endif

#define SHARED_BUFFER_SIZE SHARED_CTX_FIELD_1_SIZE
extern char g_stax_shared_buffer[SHARED_BUFFER_SIZE];

const nbgl_icon_details_t* get_app_icon(bool caller_icon);
const nbgl_icon_details_t* get_tx_icon(void);

// Global Warning struct for NBGL review flows
extern nbgl_warning_t warning;

void ui_idle(void);
void ui_settings(void);

#endif  // _UI_NBGL_H_
