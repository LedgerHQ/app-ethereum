#ifndef _UI_NBGL_H_
#define _UI_NBGL_H_

#include <nbgl_use_case.h>
#include <nbgl_page.h>

extern nbgl_page_t* pageContext;

void releaseContext(void);

void ui_idle(void);
void ui_menu_settings(void);
void ui_menu_about(void);

#endif // _UI_NBGL_H_