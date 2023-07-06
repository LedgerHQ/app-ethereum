#ifndef _UI_NBGL_H_
#define _UI_NBGL_H_

#include <nbgl_use_case.h>
#include <nbgl_page.h>
#include <shared_context.h>

#define SHARED_BUFFER_SIZE SHARED_CTX_FIELD_1_SIZE
extern char g_stax_shared_buffer[SHARED_BUFFER_SIZE];

extern nbgl_page_t* pageContext;

void releaseContext(void);

const nbgl_icon_details_t* get_app_icon(bool caller_icon);

void ui_idle(void);
void ui_menu_settings(void);
void ui_menu_about(void);

#endif  // _UI_NBGL_H_
