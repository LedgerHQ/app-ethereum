#include "common_ui.h"
#include "ui_nbgl.h"
#include "nbgl_use_case.h"
#include "glyphs.h"
#include "network.h"

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
    char *app_name = (char*)get_app_network_name();

    // In case a new clone is ran with an older ethereum app (unknown chain ID)
    if (app_name == NULL) {
        app_name = APPNAME;
    }
    nbgl_useCaseHome(app_name, &ICONGLYPH, NULL, true, ui_menu_settings, app_quit);
}
