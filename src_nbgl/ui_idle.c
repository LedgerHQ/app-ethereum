#include "common_ui.h"
#include "ui_nbgl.h"
#include "nbgl_use_case.h"
#include "glyphs.h"

nbgl_page_t* pageContext;

void releaseContext(void) {
    if (pageContext != NULL) {
        nbgl_pageRelease(pageContext);
        pageContext = NULL;
    }
}
enum { BACK_TOKEN = 0, INFO_TOKEN, NEXT_TOKEN, CANCEL_TOKEN, QUIT_INFO_TOKEN, QUIT_APP_TOKEN };

void app_quit(void) {
    // exit app here
    os_sched_exit(-1);
}

void ui_idle(void) {
    nbgl_useCaseHome(APPNAME, &ICONGLYPH, NULL, true, ui_menu_settings, app_quit);
}